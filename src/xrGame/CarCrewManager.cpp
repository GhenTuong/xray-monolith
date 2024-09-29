#include "stdafx.h"
#include "CarCrewManager.h"

SSeat::SSeat(CCar *obj)
{
	type = CCar::eCarCrewNone;
	seat_id = BI_NONE;
	door_id = BI_NONE;
	exit_position.set(0.0F, 0.0F, 0.0F);

	camera = xr_new<CCameraFirstEye>(obj, CCameraBase::flRelativeLink | CCameraBase::flPositionRigid);
	camera->tag = CCar::ectFirst;
	camera->Load("car_firsteye_cam");
	camera_bone_def = BI_NONE;
	camera_bone_aim = BI_NONE;
	zoom_factor_def = 1.0F;
	zoom_factor_aim = 1.0F;
}

SSeat::~SSeat()
{
	xr_delete(camera);
}

CCarCrewManager::CCarCrewManager(CCar *obj)
{
	m_car = obj;
}

CCarCrewManager::~CCarCrewManager()
{
	m_crew.clear();
	m_seat.clear();
}

void CCarCrewManager::Load()
{
	IKinematics *K = m_car->Visual()->dcast_PKinematics();
	CInifile *ini = K->LL_UserData();

	if (ini->section_exist("crew_section"))
	{
		int num = ini->line_count("crew_section");
		for (int idx = 0; idx < num; idx++)
		{
			LPCSTR sec;
			LPCSTR val;
			if (ini->r_line("crew_section", idx, &sec, &val) && strlen(sec) && ini->section_exist(sec))
			{
				if (m_seat.size() == 6)
				{
					Msg("%s:%d %s has too many crews. The maximum amount of crews allowed is 6.", __FUNCTION__, __LINE__, m_car->cNameSect_str());
					break;
				}

				m_seat.push_back(SSeat(m_car));
				SSeat &S = m_seat.back();

				LPCSTR tmp = READ_IF_EXISTS(ini, r_string, sec, "type", "");
				if (strcmp(sec, "driver") == 0)
					S.type = CCar::eCarCrewDriver;
				else if (strcmp(sec, "gunner") == 0)
					S.type = CCar::eCarCrewGunner;
				else if (strcmp(sec, "member") == 0)
					S.type = CCar::eCarCrewMember;
				else
					S.type = CCar::eCarCrewNone;

				S.section = sec;
				S.seat_id = ini->line_exist(sec, "seat_bone") ? K->LL_BoneID(ini->r_string(sec, "seat_bone")) : BI_NONE;
				S.door_id = ini->line_exist(sec, "door_bone") ? K->LL_BoneID(ini->r_string(sec, "door_bone")) : BI_NONE;
				S.animation = READ_IF_EXISTS(ini, r_string, sec, "animation", "norm_torso_m134_aim_0");
				S.exit_position = READ_IF_EXISTS(ini, r_fvector3, sec, "exit_position", Fvector().set(0.0f, 0.0f, 0.0f));

				S.camera->Load(READ_IF_EXISTS(ini, r_string, sec, "camera", "car_firsteye_cam"));
				S.camera_bone_def = ini->line_exist(sec, "camera_bone_def") ? K->LL_BoneID(ini->r_string(sec, "camera_bone_def")) : BI_NONE;
				S.camera_bone_aim = ini->line_exist(sec, "camera_bone_aim") ? K->LL_BoneID(ini->r_string(sec, "camera_bone_aim")) : BI_NONE;
				S.zoom_factor_def = READ_IF_EXISTS(ini, r_float, sec, "zoom_factor_def", 1.0F);
				S.zoom_factor_aim = READ_IF_EXISTS(ini, r_float, sec, "zoom_factor_aim", 1.0F);
			}
		}
	}
}

bool CCarCrewManager::Available()
{
	return m_seat.empty() == false;
}

void CCarCrewManager::Update()
{
	xr_map<CGameObject *, SSeat *>::iterator i = m_crew.begin();
	xr_map<CGameObject *, SSeat *>::iterator e = m_crew.end();
	for (; i != e; ++i)
	{
		IKinematics *K = m_car->Visual()->dcast_PKinematics();
		Fmatrix xform = Fmatrix(K->LL_GetTransform(i->second->seat_id));
		i->first->XFORM().set(Fmatrix().mul_43(m_car->XFORM(), xform));
	}
}

void CCarCrewManager::DismountAll()
{
	int cnt = 0;
	while (m_crew.size() > 0)
	{
		if (cnt > 100)
		{
			Msg("%s:%d [%s] break from a potential infinity loop. Unable to dismount all crews.", __FUNCTION__, __LINE__, m_car->cNameSect_str());
			break;
		}
		cnt = cnt + 1;

		xr_map<CGameObject *, SSeat *>::iterator i = m_crew.begin();
		if (i->first->cast_actor())
		{
			i->first->cast_actor()->use_HolderEx(NULL, false);
			m_crew.erase(i);
		}
		if (i->first->cast_stalker())
		{
			i->first->cast_stalker()->use_HolderEx(NULL);
			m_crew.erase(i);
		}
	}
}

SSeat *CCarCrewManager::GetSeatEmpty()
{
	for (auto &seat : m_seat)
	{
		if (GetCrewBySeat(&seat) == NULL)
		{
			return &seat;
		}
	}
	return NULL;
}

SSeat *CCarCrewManager::GetSeatByName(LPCSTR sec)
{
	if (sec && strlen(sec))
	{
		for (auto &seat : m_seat)
		{
			if (strcmp(seat.section, sec) == 0)
			{
				return &seat;
			}
		}
	}
	return NULL;
}

SSeat *CCarCrewManager::GetSeatByCrew(CGameObject *crew)
{
	return (crew) ? m_crew.at(crew) : NULL;
}

SSeat *CCarCrewManager::GetSeatByDoor(u16 bone_id)
{
	if (bone_id == BI_NONE)
		return NULL;
	for (auto &seat : m_seat)
	{
		if (seat.door_id == bone_id)
		{
			return &seat;
		}
	}
	return NULL;
}

CGameObject *CCarCrewManager::GetCrewBySeat(SSeat *seat)
{
	if (seat)
	{
		xr_map<CGameObject *, SSeat *>::iterator i = m_crew.begin();
		xr_map<CGameObject *, SSeat *>::iterator e = m_crew.end();
		for (; i != e; ++i)
		{
			if (i->second == seat)
			{
				return i->first;
			}
		}
	}
	return NULL;
}

bool CCarCrewManager::AttachCrew(CGameObject *obj, LPCSTR sec)
{
	/* Target seat must be unoccupied. */
	if (obj == NULL)
		return false;
	SSeat *seat = GetSeatByName(sec);
	if (seat == NULL)
		return false;
	if (GetCrewBySeat(seat))
		return false;
	DetachCrew(obj);
	m_car->OnAttachCrew(seat->type);
	m_crew.insert(mk_pair(obj, seat));
	return true;
}

void CCarCrewManager::DetachCrew(CGameObject *obj)
{
	if (obj == NULL)
		return;
	xr_map<CGameObject *, SSeat *>::iterator i = m_crew.find(obj);
	if (i == m_crew.end())
		return;
	m_car->OnDetachCrew(i->second->type);
	m_crew.erase(i);
}

void CCarCrewManager::ChangeSeat(CGameObject *obj, LPCSTR sec)
{
	/* obj must be crew in order to change seat. */
	if (obj == NULL)
		return;
	if (sec == NULL || (strlen(sec) == 0))
		return;
	SSeat *seat = GetSeatByCrew(obj);
	if (seat == NULL || strcmp(seat->section, sec) == 0)
		return;
	AttachCrew(obj, sec);
}
