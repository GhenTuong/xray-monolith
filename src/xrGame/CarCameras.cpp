#include "stdafx.h"
#pragma hdrstop
#ifdef DEBUG

#include "PHDebug.h"
#include "../xrphysics/iphworld.h"
#endif
#include "alife_space.h"
#include "hit.h"
#include "PHDestroyable.h"
#include "car.h"
#include "actor.h"
#include "cameralook.h"
#include "camerafirsteye.h"
#include "level.h"
#include "../xrEngine/cameramanager.h"

#ifdef CCAR_CHANGE
#include "Actor.h"
#include "../Include/xrRender/Kinematics.h"
#endif

bool CCar::HUDView() const
{
	return active_camera->tag == ectFirst;
}

#ifdef CCAR_CHANGE
void CCar::cam_Update(float dt, float fov)
{
	Fvector P;
	Fvector D;
	D.set(0, 0, 0);
	CCameraBase *cam = Camera();

	SSeat *seat = (m_crew_manager->Available()) ? m_crew_manager->GetSeatByCrew(OwnerActor()) : NULL;
	switch (cam->tag)
	{
	case ectFirst:
		if (seat)
		{
			u16 bone_id = (IsCameraZoom() && (seat->camera_bone_aim != BI_NONE)) ? seat->camera_bone_aim : seat->camera_bone_def;
			XFORM().transform_tiny(P, Visual()->dcast_PKinematics()->LL_GetTransform(bone_id).c);
		}
		else
		{
			XFORM().transform_tiny(P, m_camera_position);
		}
		if (OwnerActor())
			OwnerActor()->Orientation().yaw = -cam->yaw;
		if (OwnerActor())
			OwnerActor()->Orientation().pitch = -cam->pitch;
		break;
	case ectChase:
		XFORM().transform_tiny(P, m_camera_position);
		if (OwnerActor())
			OwnerActor()->Orientation().yaw = -cam->yaw;
		if (OwnerActor())
			OwnerActor()->Orientation().pitch = -cam->pitch;
		break;
	case ectFree:
		XFORM().transform_tiny(P, m_camera_position);
		if (OwnerActor())
			OwnerActor()->Orientation().yaw = 0;
		if (OwnerActor())
			OwnerActor()->Orientation().pitch = 0;
		break;
	}

	cam->f_fov = fov;
	cam->Update(P, D);
	Level().Cameras().UpdateFromCamera(cam);
}
#else
void CCar::cam_Update(float dt, float fov)
{
#ifdef DEBUG
	VERIFY(!physics_world()->Processing());
#endif
	Fvector P, Da;
	Da.set(0, 0, 0);
	//bool							owner = !!Owner();

	XFORM().transform_tiny(P, m_camera_position);

	switch (active_camera->tag)
	{
	case ectFirst:
		// rotate head
		if (OwnerActor()) OwnerActor()->Orientation().yaw = -active_camera->yaw;
		if (OwnerActor()) OwnerActor()->Orientation().pitch = -active_camera->pitch;
		break;
	case ectChase: break;
	case ectFree: break;
	}
	active_camera->f_fov = fov;
	active_camera->Update(P, Da);
	Level().Cameras().UpdateFromCamera(active_camera);
}
#endif

void CCar::OnCameraChange(int type)
{
	if (Owner())
	{
		if (type == ectFirst)
		{
			Owner()->setVisible(FALSE);
		}
		else if (active_camera->tag == ectFirst)
		{
			Owner()->setVisible(TRUE);
		}
	}

	if (!active_camera || active_camera->tag != type)
	{

#ifdef CCAR_CHANGE
		SSeat *seat = (m_crew_manager->Available()) ? m_crew_manager->GetSeatByCrew(OwnerActor()) : NULL;
		if (seat && type == ectFirst)
		{
			active_camera = seat->camera;
		}
		else
		{
			active_camera = camera[type];
		}
#else
		active_camera = camera[type];
#endif

		if (ectFree == type)
		{
			Fvector xyz;
			XFORM().getXYZi(xyz);
			active_camera->yaw = xyz.y;
		}
	}
}
