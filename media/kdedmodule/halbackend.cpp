/* This file is part of the KDE Project
   Copyright (c) 2004 J�r�me Lodewyck <lodewyck@clipper.ens.fr>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include "halbackend.h"

#include <klocale.h>
#include <kurl.h>
#include <kdebug.h>

#define MOUNT_SUFFIX	(hal_volume_is_mounted(halVolume) ? QString("_mounted") : QString("_unmounted"))

/* Static instance of this class, for static HAL callbacks */
static HALBackend* s_HALBackend;

/* A macro function to convert HAL string properties to QString */
QString hal_device_get_property_QString(LibHalContext *ctx, const char* udi, const char *key)
{
	char*   _ppt_string;
	QString _ppt_QString;
	_ppt_string = hal_device_get_property_string(ctx, udi, key);
	_ppt_QString = QString(_ppt_string ? _ppt_string : "");
	hal_free_string(_ppt_string);
	return _ppt_QString;
}

/* Constructor */
HALBackend::HALBackend(MediaList &list, QObject* parent)
	: QObject()
	, BackendBase(list)
	, m_halContext(NULL)
	, m_parent(parent)
{
	s_HALBackend = this;
}

/* Destructor */
HALBackend::~HALBackend()
{
	/* Close HAL connection */
	if (m_halContext)
		hal_shutdown(m_halContext);
	if (m_halStoragePolicy)
		hal_storage_policy_free(m_halStoragePolicy);
}

/* Connect to the HAL */
bool HALBackend::InitHal()
{
	/* libhal initialization */
	m_halFunctions.main_loop_integration	= HALBackend::hal_main_loop_integration;
	m_halFunctions.device_added				= HALBackend::hal_device_added;
	m_halFunctions.device_removed			= HALBackend::hal_device_removed;
	m_halFunctions.device_new_capability	= NULL;
	m_halFunctions.device_lost_capability	= NULL;
	m_halFunctions.device_property_modified	= HALBackend::hal_device_property_modified;
	m_halFunctions.device_condition			= HALBackend::hal_device_condition;

	m_halContext = hal_initialize(&m_halFunctions, FALSE);
	if (!m_halContext)
	{
		kdDebug()<<"Failed to initialize HAL!"<<endl;
		return false;
	}

	/** @todo customize watch policy */
	if (hal_device_property_watch_all(m_halContext))
	{
		kdDebug()<<"Failed to watch HAL properties!"<<endl;
		return false;
	}

	/* libhal-storage initialization */
	m_halStoragePolicy = hal_storage_policy_new();
	/** @todo define libhal-storage icon policy */

	/* List devices at startup */
	return ListDevices();
}

/* List devices (at startup)*/
bool HALBackend::ListDevices()
{
	int numDevices;
	char** halDeviceList = hal_get_all_devices(m_halContext, &numDevices);

	if (!halDeviceList)
		return false;

	kdDebug() << "HALBackend::ListDevices : " << numDevices << " devices found" << endl;
	for (int i = 0; i < numDevices; i++)
		AddDevice(halDeviceList[i]);

	return true;
}

/* Create a media instance for the HAL device "udi".
This functions checks whether the device is worth listing */
void HALBackend::AddDevice(const char *udi)
{
	/* We don't deal with devices that do not expose their capabilities.
	If we don't check this, we will get a lot of warning messages from libhal */
	if (!hal_device_property_exists(m_halContext, udi, "info.capabilities"))
		return;

	/* If the device is already listed, do not process.
	This should not happen, but who knows... */
	/** @todo : refresh properties instead ? */
	if (m_mediaList.findById(udi))
		return;

	/* Add volume block devices */
	if (hal_device_query_capability(m_halContext, udi, "volume"))
	{
		/* We only list volume that have a filesystem or volume that have an audio track*/
		if ( (hal_device_get_property_QString(m_halContext, udi, "volume.fsusage") != "filesystem") &&
		     (!hal_device_get_property_bool(m_halContext, udi, "volume.disc.has_audio")) )
			return;
		/* Query drive udi */
		QString driveUdi = hal_device_get_property_QString(m_halContext, udi, "block.storage_device");
		/* We don't list floppy volumes because we list floppy drives */
		if (hal_device_get_property_QString(m_halContext, driveUdi.ascii(), "storage.drive_type") == "floppy" )
			return;

		/** @todo check exclusion list **/

		/* Create medium */
		Medium* medium = new Medium(udi, "");
		setVolumeProperties(medium);
		m_mediaList.addMedium(medium);

		return;
	}

	/* Floppy drives */
	if (hal_device_query_capability(m_halContext, udi, "storage"))
		if (hal_device_get_property_QString(m_halContext, udi, "storage.drive_type") == "floppy")
		{
			/* Create medium */
			Medium* medium = new Medium(udi, "");
			setFloppyProperties(medium);
			m_mediaList.addMedium(medium);
			return;
		}

	/* Camera handled by gphoto2*/
	if (hal_device_query_capability(m_halContext, udi, "camera"))

		{
			/* Create medium */
			Medium* medium = new Medium(udi, "");
			setCameraProperties(medium);
			m_mediaList.addMedium(medium);
			return;
		}
}

void HALBackend::RemoveDevice(const char *udi)
{
	m_mediaList.removeMedium(udi);
}

void HALBackend::ModifyDevice(const char *udi, const char* key)
{
	Q_UNUSED(udi);
	Q_UNUSED(key);
/*
	TODO: enable this when the watch policy is written
*/
}

void HALBackend::DeviceCondition(const char* udi, const char* condition)
{
	const char* mediumUdi = findMediumUdiFromUdi(udi);
	if (!mediumUdi)
		return;

	QString conditionName = QString(condition);
	kdDebug() << "Processing device condition " << conditionName << " for " << udi << endl;

	/* TODO: Warn the user that (s)he should unmount devices before unplugging */
	if (conditionName == "VolumeUnmountForced")
		ResetProperties(mediumUdi);

	/* Reset properties after mounting */
	if (conditionName == "VolumeMount")
		ResetProperties(mediumUdi);

	/* Reset properties after unmounting */
	if (conditionName == "VolumeUnmount")
		ResetProperties(mediumUdi);
}

void HALBackend::MainLoopIntegration(DBusConnection *dbusConnection)
{
	m_dBusQtConnection = new DBusQt::Connection(m_parent);
	m_dBusQtConnection->dbus_connection_setup_with_qt_main(dbusConnection);
}

/******************************************
** Properties attribution                **
******************************************/

/* Return the medium udi that should be updated when recieving a call for
device udi */
const char* HALBackend::findMediumUdiFromUdi(const char* udi)
{
	/* Easy part : this Udi is already registered as a device */
	const Medium* medium = m_mediaList.findById(udi);
	if (medium)
		return medium->id().ascii();

	/* Hard part : this is a volume whose drive is registered */
	if (hal_device_property_exists(m_halContext, udi, "info.capabilities"))
		if (hal_device_query_capability(m_halContext, udi, "volume"))
		{
			QString driveUdi = hal_device_get_property_QString(m_halContext, udi, "block.storage_device");
			return findMediumUdiFromUdi(driveUdi.ascii());
		}

	return NULL;
}

void HALBackend::ResetProperties(const char* mediumUdi)
{
	kdDebug() << "HALBackend::setProperties" << endl;

	Medium* m = new Medium(mediumUdi, "");
	if (hal_device_query_capability(m_halContext, mediumUdi, "volume"))
		setVolumeProperties(m);
	if (hal_device_query_capability(m_halContext, mediumUdi, "storage"))
		setFloppyProperties(m);
	if (hal_device_query_capability(m_halContext, mediumUdi, "camera"))
		setCameraProperties(m);

	m_mediaList.changeMediumState(*m);

	delete m;
}

void HALBackend::setVolumeProperties(Medium* medium)
{
	kdDebug() << "HALBackend::setVolumeProperties for " << medium->id() << endl;

	const char* udi = medium->id().ascii();
	/* Check if the device still exists */
	if (!hal_device_exists(m_halContext, udi))
			return;

	/* Get device information from libhal-storage */
	HalVolume* halVolume = hal_volume_from_udi(m_halContext, udi);
	QString driveUdi = hal_volume_get_storage_device_udi(halVolume);
	HalDrive*  halDrive  = hal_drive_from_udi(m_halContext, driveUdi.ascii());

	medium->setName(
		generateName(hal_volume_get_device_file(halVolume)) );

	medium->mountableState(
		hal_volume_get_device_file(halVolume),		/* Device node */
		hal_volume_get_mount_point(halVolume),		/* Mount point */
		hal_volume_get_fstype(halVolume),			/* Filesystem type */
		hal_volume_is_mounted(halVolume) );			/* Mounted ? */

	QString mimeType;
	if (hal_volume_is_disc(halVolume))
	{
		mimeType = "media/cdrom" + MOUNT_SUFFIX;

		HalVolumeDiscType discType = hal_volume_get_disc_type(halVolume);
		if ((discType == HAL_VOLUME_DISC_TYPE_CDR) || (discType == HAL_VOLUME_DISC_TYPE_CDRW))
			if (hal_volume_disc_is_blank(halVolume))
			{
				mimeType = "media/blankcd";
				medium->unmountableState("");
			}
			else
				mimeType = "media/cdwriter" + MOUNT_SUFFIX;

		if ((discType == HAL_VOLUME_DISC_TYPE_DVDROM) || (discType == HAL_VOLUME_DISC_TYPE_DVDRAM) ||
			(discType == HAL_VOLUME_DISC_TYPE_DVDR) || (discType == HAL_VOLUME_DISC_TYPE_DVDRW) ||
			(discType == HAL_VOLUME_DISC_TYPE_DVDPLUSR) || (discType == HAL_VOLUME_DISC_TYPE_DVDPLUSRW) )
			if (hal_volume_disc_is_blank(halVolume))
			{
				mimeType = "media/blankdvd";
				medium->unmountableState("");
			}
			else
				mimeType = "media/dvd" + MOUNT_SUFFIX;

		if (hal_volume_disc_has_audio(halVolume) && !hal_volume_disc_has_data(halVolume))
		{
			mimeType = "media/audiocd";
			medium->unmountableState( "audiocd:/?device=" + QString(hal_volume_get_device_file(halVolume)) );
		}

		/** @todo check if the disc id a vcd or a video dvd (only for mounted volumes) */
	}
	else
	{
		mimeType = "media/hdd" + MOUNT_SUFFIX;
		if (hal_drive_is_hotpluggable(halDrive))
			mimeType = "media/removable" + MOUNT_SUFFIX;
	}
	medium->setMimeType(mimeType);

	medium->setIconName(QString::null);

	medium->setLabel(QString::fromUtf8( hal_volume_policy_compute_display_name(halDrive,
		halVolume, m_halStoragePolicy) ) );

	hal_drive_free(halDrive);
	hal_volume_free(halVolume);
}

void HALBackend::setFloppyProperties(Medium* medium)
{
	kdDebug() << "HALBackend::setFloppyProperties for " << medium->id() << endl;

	const char* udi = medium->id().ascii();
	/* Check if the device still exists */
	if (!hal_device_exists(m_halContext, udi))
		return;

	HalDrive*  halDrive  = hal_drive_from_udi(m_halContext, udi);
	int numVolumes;
	char** volumes = hal_drive_find_all_volumes(m_halContext, halDrive, &numVolumes);
	HalVolume* halVolume = NULL;
	kdDebug() << " found " << numVolumes << " volumes" << endl;
	if (numVolumes)
		halVolume = hal_volume_from_udi(m_halContext, volumes[0]);

	medium->setName(
		generateName(hal_drive_get_device_file(halDrive)) );

	if (halVolume)
	{
		medium->mountableState(
			hal_volume_get_device_file(halVolume),		/* Device node */
			hal_volume_get_mount_point(halVolume),		/* Mount point */
			hal_volume_get_fstype(halVolume),			/* Filesystem type */
			hal_volume_is_mounted(halVolume) );			/* Mounted ? */
	}
	else
	{
		medium->mountableState(
			hal_drive_get_device_file(halDrive),		/* Device node */
			"",											/* Mount point */
			"",											/* Filesystem type */
			false );									/* Mounted ? */
	}

	if (halVolume)
		medium->setMimeType("media/floppy" + MOUNT_SUFFIX);
	else
		medium->setMimeType("media/floppy_unmounted");

	medium->setIconName(QString::null);

	medium->setLabel(QString::fromUtf8( hal_drive_policy_compute_display_name(halDrive,
		halVolume, m_halStoragePolicy) ) );

	hal_drive_free(halDrive);
	hal_volume_free(halVolume);
}

void HALBackend::setCameraProperties(Medium* medium)
{
	kdDebug() << "HALBackend::setCameraProperties for " << medium->id() << endl;

	const char* udi = medium->id().ascii();
	/* Check if the device still exists */
	if (!hal_device_exists(m_halContext, udi))
		return;

	/** @todo find name */
	medium->setName("camera");
	/** @todo find the rest of this URL */
	medium->unmountableState("camera:/");
	medium->setMimeType("media/gphoto2camera");
	medium->setIconName(QString::null);
	/** @todo find label */
	medium->setLabel("Camera");
}

QString HALBackend::generateName(const QString &devNode)
{
	return KURL(devNode).fileName();
}

/******************************************
** HAL CALL-BACKS                        **
******************************************/

void HALBackend::hal_main_loop_integration(LibHalContext *ctx,
			DBusConnection *dbus_connection)
{
	kdDebug() << "HALBackend::hal_main_loop_integration" << endl;
	Q_UNUSED(ctx);
	s_HALBackend->MainLoopIntegration(dbus_connection);
}

void HALBackend::hal_device_added(LibHalContext *ctx, const char *udi)
{
	kdDebug() << "HALBackend::hal_device_added " << udi <<  endl;
	Q_UNUSED(ctx);
	s_HALBackend->AddDevice(udi);
}

void HALBackend::hal_device_removed(LibHalContext *ctx, const char *udi)
{
	kdDebug() << "HALBackend::hal_device_removed " << udi << endl;
	Q_UNUSED(ctx);
	s_HALBackend->RemoveDevice(udi);
}

void HALBackend::hal_device_property_modified(LibHalContext *ctx, const char *udi,
			const char *key, dbus_bool_t is_removed, dbus_bool_t is_added)
{
	kdDebug() << "HALBackend::hal_property_modified " << udi << " -- " << key << endl;
	Q_UNUSED(ctx);
	Q_UNUSED(is_removed);
	Q_UNUSED(is_added);
	s_HALBackend->ModifyDevice(udi, key);
}
void HALBackend::hal_device_condition(LibHalContext *ctx, const char *udi,
			const char *condition_name, DBusMessage *message)
{
	kdDebug() << "HALBackend::hal_device_condition " << udi << " -- " << condition_name << endl;
	Q_UNUSED(ctx);
	Q_UNUSED(message);
	s_HALBackend->DeviceCondition(udi, condition_name);
}

#include "halbackend.moc"