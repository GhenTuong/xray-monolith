#include "stdafx.h"
#include "CarCrewManager.h"

SSeat::SSeat(CCar *obj)
{
	type = CCar::eCarCrewNone;
	seat_id = BI_NONE;
	door_id = BI_NONE;
	exit_position.set(0.0F, 0.0F, 0.0F);

	anim_play = NULL;
	anim_idle = NULL;
	anim_fire = NULL;
	anim_ns = NULL;
	anim_ls = NULL;
	anim_rs = NULL;

	camera = xr_new<CCameraFirstEye>(obj, CCameraBase::flRelativeLink | CCameraBase::flPositionRigid);
	camera->tag = CCar::ectFirst;
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
	Clear();
}

void CCarCrewManager::Load()
{
	IKinematics *K = m_car->Visual()->dcast_PKinematics();
	CInifile *ini = K->LL_UserData();

	Clear();
	if (ini->section_exist("crew_section"))
	{
		int num = ini->line_count("crew_section");
		for (int idx = 0; idx < num; idx++)
		{
			LPCSTR sec = NULL;
			LPCSTR val = NULL;
			if (ini->r_line("crew_section", idx, &sec, &val) && sec && strlen(sec) && ini->section_exist(sec))
			{
				if (m_seat.size() == 6)
				{
					Msg("%s:%d %s has too many crews. The maximum amount of crews allowed is 6.", __FUNCTION__, __LINE__, m_car->cNameSect_str());
					break;
				}
				m_seat.insert(mk_pair(m_seat.size(), xr_new<SSeat>(m_car)));
				SSeat *S = m_seat.at(m_seat.size() - 1);

				LPCSTR tmp = READ_IF_EXISTS(ini, r_string, sec, "type", "");
				if (strcmp(tmp, "member") == 0)
					S->type = CCar::eCarCrewMember;
				else if (strcmp(tmp, "driver") == 0)
					S->type = CCar::eCarCrewDriver;
				else if (strcmp(tmp, "gunner") == 0)
					S->type = CCar::eCarCrewGunner;
				else
					S->type = CCar::eCarCrewNone;

				S->section = sec;
				S->seat_id = ini->line_exist(sec, "seat_bone") ? K->LL_BoneID(ini->r_string(sec, "seat_bone")) : BI_NONE;
				S->door_id = ini->line_exist(sec, "door_bone") ? K->LL_BoneID(ini->r_string(sec, "door_bone")) : BI_NONE;
				S->exit_position = READ_IF_EXISTS(ini, r_fvector3, sec, "exit_position", Fvector().set(0.0F, 0.0F, 0.0F));

				S->anim_idle = READ_IF_EXISTS(ini, r_string, sec, "anim_idle", "norm_torso_m134_aim_0");
				S->anim_fire = READ_IF_EXISTS(ini, r_string, sec, "anim_fire", "norm_torso_m134_attack_0");
				S->anim_ns = READ_IF_EXISTS(ini, r_string, sec, "anim_ns", "steering_torso_idle");
				S->anim_ls = READ_IF_EXISTS(ini, r_string, sec, "anim_ls", "steering_torso_ls");
				S->anim_rs = READ_IF_EXISTS(ini, r_string, sec, "anim_rs", "steering_torso_rs");

				LPCSTR camera_sec = READ_IF_EXISTS(ini, r_string, sec, "camera", "car_firsteye_cam");
				S->camera->Load(camera_sec);
				S->camera_bone_def = ini->line_exist(sec, "camera_bone_def") ? K->LL_BoneID(ini->r_string(sec, "camera_bone_def")) : BI_NONE;
				S->camera_bone_aim = ini->line_exist(sec, "camera_bone_aim") ? K->LL_BoneID(ini->r_string(sec, "camera_bone_aim")) : BI_NONE;
				S->zoom_factor_def = READ_IF_EXISTS(ini, r_float, sec, "zoom_factor_def", 1.0F);
				S->zoom_factor_aim = READ_IF_EXISTS(ini, r_float, sec, "zoom_factor_aim", 1.0F);
#if 1
				Msg("%s:%d [%s] load crew [%d][%s] --------------------------------------------------", __FUNCTION__, __LINE__, m_car->cNameSect_str(), m_seat.size() - 1, sec);
				Msg("type = %d", S->type);
				Msg("seat_id = %s", K->LL_BoneName_dbg(S->seat_id));
				Msg("door_id = %s", K->LL_BoneName_dbg(S->door_id));
				Msg("anim_idle = %s", S->anim_idle);
				Msg("anim_fire = %s", S->anim_fire);
				Msg("anim_ns = %s", S->anim_ns);
				Msg("anim_ls = %s", S->anim_ls);
				Msg("anim_rs = %s", S->anim_rs);
				Msg("exit_position = %.2f,%.2f,%.2f", S->exit_position.x, S->exit_position.y, S->exit_position.z);

				Msg("camera = %s", camera_sec);
				Msg("camera_bone_def = %s", K->LL_BoneName_dbg(S->camera_bone_def));
				Msg("camera_bone_aim = %s", K->LL_BoneName_dbg(S->camera_bone_aim));
				Msg("zoom_factor_def = %.2f", S->zoom_factor_def);
				Msg("zoom_factor_aim = %.2f", S->zoom_factor_aim);
#endif
			}
		}
	}
}

void CCarCrewManager::Clear()
{
	m_crew.clear();
	for (auto &i : m_seat)
	{
		xr_delete(i.second);
	}
	m_seat.clear();
}

bool CCarCrewManager::Available()
{
	return m_seat.empty() == false;
}

void CCarCrewManager::Update()
{
	for (auto &i : m_crew)
	{
		IKinematics *K = m_car->Visual()->dcast_PKinematics();
		Fmatrix xform = Fmatrix(K->LL_GetTransform(i.second->seat_id));
		i.first->XFORM().set(Fmatrix().mul_43(m_car->XFORM(), xform));

		LPCSTR anim = NULL;
		if (i.second->type == CCar::eCarCrewDriver)
		{
			switch (m_car->e_state_steer)
			{
			case CCar::right:
				anim = i.second->anim_rs;
				break;
			case CCar::idle:
				anim = i.second->anim_ns;
				break;
				anim = i.second->anim_ls;
			case CCar::left:
				break;
			default:
				break;
			}
		}
		else if (i.second->type == CCar::eCarCrewGunner)
		{
			// anim = (m_car_weapon && m_car_weapon->IsWorking()) ? i.second->anim_fire : i.second->anim_idle;
		}

		if (anim && (anim != i.second->anim_play))
		{
			i.first->Visual()->dcast_PKinematicsAnimated()->PlayCycle(anim, FALSE);
			i.second->anim_play = anim;
		}
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
			continue;
		}
		if (i->first->cast_stalker())
		{
			i->first->cast_stalker()->use_HolderEx(NULL);
			continue;
		}
	}
}

SSeat *CCarCrewManager::GetSeatEmpty()
{
	for (auto &i : m_seat)
	{
		if (GetCrewBySeat(i.second) == NULL)
		{
			return i.second;
		}
	}
	return NULL;
}

SSeat *CCarCrewManager::GetSeatByName(LPCSTR sec)
{
	if (sec && strlen(sec))
	{
		for (auto &i : m_seat)
		{
			if (strcmp(i.second->section, sec) == 0)
			{
				return i.second;
			}
		}
	}
	return NULL;
}

SSeat *CCarCrewManager::GetSeatByCrew(CGameObject *crew)
{
	xr_map<CGameObject *, SSeat *>::iterator i = m_crew.find(crew);
	return (i != m_crew.end()) ? i->second : NULL;
}

SSeat *CCarCrewManager::GetSeatByDoor(u16 bone_id)
{
	if (bone_id == BI_NONE)
		return NULL;
	for (auto &i : m_seat)
	{
		if (i.second->door_id == bone_id)
		{
			return i.second;
		}
	}
	return NULL;
}

CGameObject *CCarCrewManager::GetCrewBySeat(SSeat *seat)
{
	if (seat)
	{
		for (auto &i : m_crew)
		{
			if (i.second == seat)
			{
				return i.first;
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
	seat->anim_play = NULL;
	m_crew.insert(mk_pair(obj, seat));
	Msg("%s:%d [%s] -> %s", __FUNCTION__, __LINE__, obj->cNameSect_str(), seat->section);
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
	i->second->anim_play = NULL;
	m_crew.erase(i);
	Msg("%s:%d [%s]", __FUNCTION__, __LINE__, obj->cNameSect_str());
}

void CCarCrewManager::CrewChangeSeat(CGameObject *obj, LPCSTR sec)
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
