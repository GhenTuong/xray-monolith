#pragma once
#include "stdafx.h"

#include "Car.h"
#include "Actor.h"
#include "ai\stalker\ai_stalker.h"
#include "alife_space.h"
#include "level.h"
#include "CameraFirstEye.h"

#include "../Include/xrRender/Kinematics.h"

class CCar;

struct SSeat
{
	LPCSTR section;
	u16 type;
	u16 seat_id;
	u16 door_id;
	Fvector exit_position;

	LPCSTR anim_play;
	LPCSTR anim_idle;
	LPCSTR anim_fire;
	LPCSTR anim_ns;
	LPCSTR anim_ls;
	LPCSTR anim_rs;

	CCameraBase *camera;
	u16 camera_bone_def;
	u16 camera_bone_aim;
	float zoom_factor_def;
	float zoom_factor_aim;
	SSeat(CCar *obj);
	~SSeat();
	CCameraBase *Camera() { return camera; }
};

class CCarCrewManager
{
private:
	CCar *m_car;
	xr_map<u16, SSeat *> m_seat;
	xr_map<CGameObject *, SSeat *> m_crew;
	void Clear();

public:
	CCarCrewManager(CCar *obj);
	~CCarCrewManager();
	void Load();
	bool Available();
	void Update();
	void DismountAll();

	SSeat *GetSeatEmpty();
	SSeat *GetSeatByName(LPCSTR sec);
	SSeat *GetSeatByCrew(CGameObject *crew);
	SSeat *GetSeatByDoor(u16 bone_id);
	CGameObject *GetCrewBySeat(SSeat *seat);
	CGameObject *GetCrewByType(u16 type);

	bool AttachCrew(CGameObject *obj, LPCSTR sec);
	void DetachCrew(CGameObject *obj);
	void CrewChangeSeat(CGameObject *obj, LPCSTR sec);
};