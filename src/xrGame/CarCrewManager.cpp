#include "stdafx.h"
#include "CarCrewManager.h"

SSeat::SSeat(CCar *obj)
{
	camera = xr_new<CCameraFirstEye>(obj, CCameraBase::flRelativeLink | CCameraBase::flPositionRigid);
	camera->tag = ectFirst;
	camera->Load("car_firsteye_cam");
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
	CInifile *ini = m_car->->Visual()->LL_UserData();
	IKinematics *K = m_car->Visual()->dcast_PKinematics();

	if (ini->section_exist("crew_section"))
	{
		int num = ini->line_count("crew_section");
		for (int idx = 0; idx < num; idx++)
		{
			LPCSTR sec;
			LPCSTR val;
			if (ini->r_line("crew_section", idx, &sec, &val) && strlen(sec) && ini->section_exist(sec))
			{
				if (m_seat->size() == 6)
				{
					Msg("%s:%d %s has too many crews. The maximum amount of crews allowed is 6.", __FUNCTION__, __LINE__, m_car->cNameSect_str());
					break;
				}

				m_seat.push_back(SSeat(m_car));
				SSeat &S = m_seat.back();

				LPCSTR tmp = READ_IF_EXISTS(ini, r_string, sec, "type", "");
				if (strcmp(sec, "member") == 0)
					S.type = CCar::eCarCrewMember;
				else if (strcmp(sec, "driver") == 0)
					S.type = CCar::eCarCrewDriver;
				else if (strcmp(sec, "gunner") == 0)
					S.type = CCar::eCarCrewGunner;
				else
					S.type = CCar::eCarCrewMember;

				S.section = sec;
				S.seat_id = ini->line_exist(sec, "seat_bone") ? K->LL_BoneID(ini->r_string(sec, "seat_bone")) : BI_NONE;
				S.door_id = ini->line_exist(sec, "door_bone") ? K->LL_BoneID(ini->r_string(sec, "door_bone")) : BI_NONE;
				S.animation = READ_IF_EXISTS(ini, r_string, sec, "animation", "norm_torso_m134_aim_0");
				S.exit_position = READ_IF_EXISTS(ini, r_string, sec, "exit_position", Fvector().set(0.0f, 0.0f, 0.0f));

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

SSeat *CCarCrewManager::GetSeatEmpty()
{
	xr_vector<SSeat>::iterator i = m_seat.begin();
	xr_vector<SSeat>::iterator e = m_seat.end();
	for (; i != e; ++i)
	{
		if (GetCrewBySeat(i) == NULL)
		{
			return i;
		}
	}
	return NULL;
}

SSeat *CCarCrewManager::GetSeatByName(LPCSTR sec)
{
	if (sec && strlen(sec))
	{
		xr_vector<SSeat>::iterator i = m_seat.begin();
		xr_vector<SSeat>::iterator e = m_seat.end();
		for (; i != e; ++i)
		{
			if (strcmp(i->section, sec) == 0)
			{
				return i;
			}
		}
	}
	return NULL;
}

SSeat *CCarCrewManager::GetSeatByCrew(CGameObject *crew)
{
	return (crew) ? m_crew.at(crew) : NULL;
}

SSeat *CCarCrewManager::GetSeatByDoor(u16 door)
{
	xr_vector<SSeat>::iterator i = m_seat.begin();
	xr_vector<SSeat>::iterator e = m_seat.end();
	for (; i != e; ++i)
	{
		if (i->door_id == door)
		{
			return i;
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
	if (obj == NULL)
		return false;
	SSeat *seat = GetSeatByName(sec);
	if (seat == NULL)
		return false;
	if (GetCrewBySeat(seat))
		return false;
	DetachCrew(obj);
	m_crew.insert(mk_pair(obj, seat));
	return true;
}

void CCarCrewManager::DetachCrew(CGameObject *obj)
{
	if (obj == NULL)
		return;
	xr_map<CGameObject *, SSeat *>::iterator i = m_crew.at(obj);
	if (i == m_crew.end())
		return;
	m_crew.erase(i);
}

bool CCarCrewManager::SwitchCrewToSeatPrev(CGameObject *crew)
{
	if (crew == NULL)
		return false;

	SSeat *seat = GetSeatByCrew(crew);
	if (seat == NULL)
		return false;

	bool passed = false;
	xr_vector<SSeat>::iterator i = m_seat.end();
	xr_vector<SSeat>::iterator e = m_seat.begin();
	for (; i != e; --i)
	{
		if (i == seat)
		{
			passed = true;
			continue;
		}
		if (passed)
		{
			if (GetCrewBySeat(i) == NULL)
			{
				AttachCrew(crew, i);
				return true;
			}
		}
	}
	return false;
}

bool CCarCrewManager::SwitchCrewToSeatNext(CGameObject *crew)
{
	if (crew == NULL)
		return false;

	SSeat *seat = GetSeatByCrew(crew);
	if (seat == NULL)
		return false;

	bool passed = false;
	xr_vector<SSeat>::iterator i = m_seat.begin();
	xr_vector<SSeat>::iterator e = m_seat.end();
	for (; i != e; ++i)
	{
		if (i == seat)
		{
			passed = true;
			continue;
		}
		if (passed)
		{
			if (GetCrewBySeat(i) == NULL)
			{
				AttachCrew(crew, i);
				return true;
			}
		}
	}
	return false;
}
