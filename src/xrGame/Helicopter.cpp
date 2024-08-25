#include "pch_script.h"
#include "helicopter.h"
#include "xrserver_objects_alife.h"
#include "../xrphysics/PhysicsShell.h"
#include "level.h"
#include "ai_sounds.h"
#include "../Include/xrRender/Kinematics.h"
#include "../Include/xrRender/KinematicsAnimated.h"
#include "script_callback_ex.h"
#include "game_object_space.h"
#include "script_game_object.h"
#include "../xrEngine/LightAnimLibrary.h"
//#include "physicscommon.h"
#include "ui_base.h"
//50fps fixed
float STEP = 0.02f;

#ifdef CHELICOPTER_CHANGE
#include "../xrphysics/IPHWorld.h"
#include "../xrphysics/PhysicsCommon.h"
#include "ai_space.h"
#include "alife_simulator.h"
#include "alife_schedule_registry.h"
#include "alife_graph_registry.h"
#include "alife_object_registry.h"

#include "Level.h"
#include "Actor.h"
#include "ActorEffector.h"
#include "CameraFirstEye.h"
#include "cameralook.h"
#include "xr_level_controller.h"
#endif

CHelicopter::CHelicopter()
{
	m_pParticle = NULL;
	m_light_render = NULL;
	m_lanim = NULL;

	ISpatial* self = smart_cast<ISpatial*>(this);
	if (self) self->spatial.type |= STYPE_VISIBLEFORAI;

	m_movement.parent = this;
	m_body.parent = this;

#ifdef CHELICOPTER_CHANGE
	camera[ectFirst] = xr_new<CCameraFirstEye>(this, CCameraBase::flRelativeLink | CCameraBase::flPositionRigid);
	camera[ectFirst]->tag = ectFirst;
	camera[ectFirst]->Load("car_firsteye_cam");

	camera[ectChase] = xr_new<CCameraLook>(this, CCameraBase::flRelativeLink);
	camera[ectChase]->tag = ectChase;
	camera[ectChase]->Load("car_look_cam");

	OnCameraChange(ectFirst);

	m_heli_type = eHeliTypeDefault;

	m_camera_bone_def = BI_NONE;
	m_camera_bone_aim = BI_NONE;
	m_zoom_factor_def = 1.0F;
	m_zoom_factor_aim = 1.0F;
	m_zoom_status = false;

	m_control_ele = eControlEle_NA;
	m_control_yaw = eControlYaw_NA;
	m_control_pit = eControlPit_NA;
	m_control_rol = eControlRol_NA;

	m_control_ele_scale = 1.0F;
	m_control_yaw_scale = 1.0F;
	m_control_pit_scale = 1.0F;
	m_control_rol_scale = 1.0F;

	bone_map = BONE_P_MAP();
	m_body_bone = BI_NONE;

	m_rotor_force_max = 0.0F;
	m_rotor_speed_max = 0.0F;

	m_engine_on = false;
	m_on_before_hit_callback = NULL;
	m_on_before_use_callback = NULL;
#endif
}

CHelicopter::~CHelicopter(){}

void CHelicopter::setState(CHelicopter::EHeliState s)
{
	m_curState = s;

#ifdef CHELICOPTER_CHANGE
	xr_delete(camera[ectFirst]);
	xr_delete(camera[ectChase]);

	bone_map.clear();
	m_rotor.clear();
#endif
}


void CHelicopter::init()
{
	m_cur_rot.set(0.0f, 0.0f);
	m_tgt_rot.set(0.0f, 0.0f);
	m_bind_rot.set(0.0f, 0.0f);

	m_allow_fire = FALSE;
	m_use_rocket_on_attack = TRUE;
	m_use_mgun_on_attack = TRUE;
	m_syncronize_rocket = TRUE;
	m_min_rocket_dist = 20.0f;
	m_max_rocket_dist = 200.0f;
	m_time_between_rocket_attack = 0;
	m_last_rocket_attack = Device.dwTimeGlobal;

	SetfHealth(1.0f);
}

DLL_Pure* CHelicopter::_construct()
{
	CEntity::_construct();
	init();
	return this;
}

void CHelicopter::reinit()
{
	inherited::reinit();
	m_movement.reinit();
	m_body.reinit();
	m_enemy.reinit();
};


void CHelicopter::Load(LPCSTR section)
{
	inherited::Load(section);
	m_movement.Load(section);
	m_body.Load(section);
	m_enemy.Load(section);


	m_death_ang_vel = pSettings->r_fvector3(section, "death_angular_vel");
	m_death_lin_vel_k = pSettings->r_float(section, "death_lin_vel_koeff");

	CHitImmunity::LoadImmunities(pSettings->r_string(section, "immunities_sect"), pSettings);

	//weapons
	CShootingObject::Load(section);

	m_layered_sounds.LoadSound(section, "snd_shoot", "sndShoot", false, ESoundTypes::SOUND_TYPE_WEAPON_SHOOTING);
	m_layered_sounds.LoadSound(section, "snd_shoot_rocket", "sndRocket", false, ESoundTypes::SOUND_TYPE_WEAPON_SHOOTING);
	m_layered_sounds.LoadSound(section, "explode_sound", "sndExplode", false, ESoundTypes::SOUND_TYPE_OBJECT_EXPLODING);

	CRocketLauncher::Load(section);

	UseFireTrail(m_enemy.bUseFireTrail); //temp force reloar disp params

	m_sAmmoType = pSettings->r_string(section, "ammo_class");
	m_CurrentAmmo.Load(*m_sAmmoType, 0);

	m_sRocketSection = pSettings->r_string(section, "rocket_class");


	m_use_rocket_on_attack = !!pSettings->r_bool(section, "use_rocket");
	m_use_mgun_on_attack = !!pSettings->r_bool(section, "use_mgun");
	m_min_rocket_dist = pSettings->r_float(section, "min_rocket_attack_dist");
	m_max_rocket_dist = pSettings->r_float(section, "max_rocket_attack_dist");
	m_min_mgun_dist = pSettings->r_float(section, "min_mgun_attack_dist");
	m_max_mgun_dist = pSettings->r_float(section, "max_mgun_attack_dist");
	m_time_between_rocket_attack = pSettings->r_u32(section, "time_between_rocket_attack");
	m_syncronize_rocket = !!pSettings->r_bool(section, "syncronize_rocket");
	m_barrel_dir_tolerance = pSettings->r_float(section, "barrel_dir_tolerance");

	//lighting & sounds
	m_smoke_particle = pSettings->r_string(section, "smoke_particle");

	m_light_range = pSettings->r_float(section, "light_range");
	m_light_brightness = pSettings->r_float(section, "light_brightness");

	m_light_color = pSettings->r_fcolor(section, "light_color");
	m_light_color.a = 1.f;
	m_light_color.mul_rgb(m_light_brightness);
	LPCSTR lanim = pSettings->r_string(section, "light_color_animmator");
	m_lanim = LALib.FindItem(lanim);

#ifdef CHELICOPTER_CHANGE
	m_drone_flag = !!READ_IF_EXISTS(pSettings, r_bool, cNameSect_str(), "is_drone", false);
	m_control_ele_scale = READ_IF_EXISTS(pSettings, r_float, cNameSect_str(), "control_ele_scale", 1.0F);
	m_control_yaw_scale = READ_IF_EXISTS(pSettings, r_float, cNameSect_str(), "control_yaw_scale", 1.0F);
	m_control_pit_scale = READ_IF_EXISTS(pSettings, r_float, cNameSect_str(), "control_pit_scale", 1.0F);
	m_control_rol_scale = READ_IF_EXISTS(pSettings, r_float, cNameSect_str(), "control_rol_scale", 1.0F);

	if (pSettings->line_exist(cNameSect_str(), "on_before_hit"))
	{
		m_on_before_hit_callback = READ_IF_EXISTS(pSettings, r_string, cNameSect_str(), "on_before_hit", NULL);
	}
	if (pSettings->line_exist(cNameSect_str(), "on_before_use"))
	{
		m_on_before_use_callback = READ_IF_EXISTS(pSettings, r_string, cNameSect_str(), "on_before_use", NULL);
	}
	if (pSettings->line_exist(cNameSect_str(), "on_before_start_engine"))
	{
		m_on_before_engine_callback = READ_IF_EXISTS(pSettings, r_string, cNameSect_str(), "on_before_engine", NULL);
	}
#endif
}

void CHelicopter::reload(LPCSTR section)
{
	inherited::reload(section);
}

#ifdef CHELICOPTER_CHANGE
void CollisionCallbackEnable(bool &do_colide, bool bo1, dContact &c, SGameMtl *material_1, SGameMtl *material_2)
{
	do_colide = true;
}
#endif

void CollisionCallbackAlife(bool& do_colide, bool bo1, dContact& c, SGameMtl* material_1, SGameMtl* material_2)
{
	do_colide = false;
}

void ContactCallbackAlife(CDB::TRI* T, dContactGeom* c)
{
}

BOOL CHelicopter::net_Spawn(CSE_Abstract* DC)
{
	SetfHealth(100.0f);
	setState(CHelicopter::eAlive);
	m_flame_started = false;
	m_light_started = false;
	m_exploded = false;
	m_ready_explode = false;
	m_dead = false;

	if (!inherited::net_Spawn(DC))
		return (FALSE);

	CPHSkeleton::Spawn((CSE_Abstract*)(DC));
	for (u32 i = 0; i < 4; ++i)
		CRocketLauncher::SpawnRocket(*m_sRocketSection, smart_cast<CGameObject*>(this));

	// assigning m_animator here
	CSE_Abstract* abstract = (CSE_Abstract*)(DC);
	CSE_ALifeHelicopter* heli = smart_cast<CSE_ALifeHelicopter*>(abstract);
	VERIFY(heli);

	R_ASSERT(Visual()&&smart_cast<IKinematics*>(Visual()));
	IKinematics* K = smart_cast<IKinematics*>(Visual());
	CInifile* pUserData = K->LL_UserData();

	m_rotate_x_bone = K->LL_BoneID(pUserData->r_string("helicopter_definition", "wpn_rotate_x_bone"));
	m_rotate_y_bone = K->LL_BoneID(pUserData->r_string("helicopter_definition", "wpn_rotate_y_bone"));
	m_fire_bone = K->LL_BoneID(pUserData->r_string("helicopter_definition", "wpn_fire_bone"));
	m_death_bones_to_hide = pUserData->r_string("on_death_mode", "scale_bone");
	m_left_rocket_bone = K->LL_BoneID(pUserData->r_string("helicopter_definition", "left_rocket_bone"));
	m_right_rocket_bone = K->LL_BoneID(pUserData->r_string("helicopter_definition", "right_rocket_bone"));

	m_smoke_bone = K->LL_BoneID(pUserData->r_string("helicopter_definition", "smoke_bone"));
	m_light_bone = K->LL_BoneID(pUserData->r_string("helicopter_definition", "light_bone"));

	CExplosive::Load(pUserData, "explosion");
	CExplosive::SetInitiator(ID());

#ifdef CHELICOPTER_CHANGE
	LPCSTR s = READ_IF_EXISTS(pUserData, r_string, "helicopter_definition", "hit_section", NULL);
	if (s && strlen(s) && pUserData->section_exist(s))
	{
		int lc = pUserData->line_count(s);
		LPCSTR name;
		LPCSTR value;
		s16 boneID;
		for (int i = 0; i < lc; ++i)
		{
			pUserData->r_line(s, i, &name, &value);
			boneID = K->LL_BoneID(name);
			m_hitBones.insert(std::make_pair(boneID, (float)atof(value)));
		}
	}
#else
	LPCSTR s = pUserData->r_string("helicopter_definition", "hit_section");

	if (pUserData->section_exist(s))
	{
		int lc = pUserData->line_count(s);
		LPCSTR name;
		LPCSTR value;
		s16 boneID;
		for (int i = 0; i < lc; ++i)
		{
			pUserData->r_line(s, i, &name, &value);
			boneID = K->LL_BoneID(name);
			m_hitBones.insert(std::make_pair(boneID, (float)atof(value)));
		}
	}
#endif

	CBoneInstance& biX = smart_cast<IKinematics*>(Visual())->LL_GetBoneInstance(m_rotate_x_bone);
	biX.set_callback(bctCustom, BoneMGunCallbackX, this);
	CBoneInstance& biY = smart_cast<IKinematics*>(Visual())->LL_GetBoneInstance(m_rotate_y_bone);
	biY.set_callback(bctCustom, BoneMGunCallbackY, this);
	CBoneData& bdX = K->LL_GetData(m_rotate_x_bone);
	VERIFY(bdX.IK_data.type==jtJoint);
	m_lim_x_rot.set(bdX.IK_data.limits[0].limit.x, bdX.IK_data.limits[0].limit.y);
	CBoneData& bdY = K->LL_GetData(m_rotate_y_bone);
	VERIFY(bdY.IK_data.type==jtJoint);
	m_lim_y_rot.set(bdY.IK_data.limits[1].limit.x, bdY.IK_data.limits[1].limit.y);

	xr_vector<Fmatrix> matrices;
	K->LL_GetBindTransform(matrices);
	m_i_bind_x_xform.invert(matrices[m_rotate_x_bone]);
	m_i_bind_y_xform.invert(matrices[m_rotate_y_bone]);
	m_bind_rot.x = matrices[m_rotate_x_bone].k.getP();
	m_bind_rot.y = matrices[m_rotate_y_bone].k.getH();
	m_bind_x.set(matrices[m_rotate_x_bone].c);
	m_bind_y.set(matrices[m_rotate_y_bone].c);

	IKinematicsAnimated* A = smart_cast<IKinematicsAnimated*>(Visual());
	if (A)
	{
			A->PlayCycle(*heli->startup_animation);
		K->CalculateBones(TRUE);
	}

#ifdef CHELICOPTER_CHANGE
	{
		LPCSTR s = READ_IF_EXISTS(pUserData, r_string, "helicopter_definition", "engine_sound", NULL);
		if (s && strlen(s))
		{
			m_engineSound.create(s, st_Effect, sg_SourceType);
			m_engineSound.play_at_pos(0, XFORM().c, sm_Looped);
		}
		else
		{
			m_engineSound.create(*heli->engine_sound, st_Effect, sg_SourceType);
			m_engineSound.play_at_pos(0, XFORM().c, sm_Looped);
		}
	}
#else
	m_engineSound.create(*heli->engine_sound, st_Effect, sg_SourceType);
	m_engineSound.play_at_pos(0, XFORM().c, sm_Looped);
#endif

	CShootingObject::Light_Create();

	setVisible(TRUE);
	setEnabled(TRUE);


	m_stepRemains = 0.0f;

	//lighting
	m_light_render = ::Render->light_create();
	m_light_render->set_shadow(false);
	m_light_render->set_type(IRender_Light::POINT);
	m_light_render->set_range(m_light_range);
	m_light_render->set_color(m_light_color);

	if (g_Alive())processing_activate();
	TurnEngineSound(false);
	if (pUserData->section_exist("destroyed"))
		CPHDestroyable::Load(pUserData, "destroyed");
#ifdef DEBUG
	Device.seqRender.Add(this,REG_PRIORITY_LOW-1);
#endif

#ifdef CHELICOPTER_CHANGE
	CInifile *ini = K->LL_UserData();

	{
		LPCSTR str = READ_IF_EXISTS(ini, r_string, "config", "heli_type", NULL);
		m_heli_type = eHeliTypeDefault;
		if (str)
		{
			if (strcmp(str, "default"))
			{
				m_heli_type = eHeliTypeDefault;
			}
			if (strcmp(str, "physic"))
			{
				m_heli_type = eHeliTypePhysic;
			}
		}
	}

	m_body_bone = ini->line_exist("config", "body_bone") ? K->LL_BoneID(ini->r_string("config", "body_bone")) : BI_NONE;

	if (ini->line_exist("config", "camera_first") && ini->section_exist(ini->r_string("config", "camera_first")))
	{
		camera[ectFirst]->Load(ini->r_string("config", "camera_first"));
	}
	if (ini->line_exist("config", "camera_chase") && ini->section_exist(ini->r_string("config", "camera_chase")))
	{
		camera[ectChase]->Load(ini->r_string("config", "camera_chase"));
	}

	m_camera_bone_def = ini->line_exist("config", "camera_bone_def") ? K->LL_BoneID(ini->r_string("config", "camera_bone_def")) : BI_NONE;
	m_camera_bone_aim = ini->line_exist("config", "camera_bone_aim") ? K->LL_BoneID(ini->r_string("config", "camera_bone_aim")) : BI_NONE;
	m_zoom_factor_def = READ_IF_EXISTS(ini, r_float, "config", "zoom_factor_def", 1.0F);
	m_zoom_factor_aim = READ_IF_EXISTS(ini, r_float, "config", "zoom_factor_aim", 1.0F);

	m_rotor.clear();
	if (ini->section_exist("config"))
	{
		LPCSTR str = READ_IF_EXISTS(ini, r_string, "config", "rotor_bone", NULL);
		string64 bone_name;
		int n = _GetItemCount(str);
		for (int k = 0; k < n; ++k)
		{
			memset(bone_name, 0, sizeof(bone_name));
			_GetItem(str, k, bone_name);
			u16 bone_id = (strlen(bone_name)) ? K->LL_BoneID(bone_name) : BI_NONE;
			if (bone_id != BI_NONE && bone_map.find(bone_id)->second.joint != NULL)
			{
				m_rotor.insert(mk_pair(bone_id, bone_map.find(bone_id)->second.joint));
			}
		}

		m_rotor_force_max = READ_IF_EXISTS(ini, r_float, "config", "rotor_force_max", 0.0F);
		m_rotor_speed_max = READ_IF_EXISTS(ini, r_float, "config", "rotor_speed_max", 0.0F) * PI_MUL_2 / 60;
	}
#endif

	return TRUE;
}

void CHelicopter::net_Destroy()
{
#ifdef CHELICOPTER_CHANGE
	if (OwnerActor())
	{
		OwnerActor()->use_HolderEx(NULL, true);
	}
	CPHUpdateObject::Deactivate();
#endif

	inherited::net_Destroy();
	CExplosive::net_Destroy();
	CShootingObject::Light_Destroy();
	CShootingObject::StopFlameParticles();
	CPHSkeleton::RespawnInit();
	CPHDestroyable::RespawnInit();
	m_engineSound.stop();
	m_brokenSound.stop();
	CParticlesObject::Destroy(m_pParticle);
	m_light_render.destroy();
	m_movement.net_Destroy();
#ifdef DEBUG
	Device.seqRender.Remove(this);
#endif
}

void CHelicopter::SpawnInitPhysics(CSE_Abstract* D)
{
#ifdef CHELICOPTER_CHANGE
	if (m_heli_type == eHeliTypePhysic)
	{
		if (!Visual())
			return;
		IRenderVisual *V = Visual();
		IKinematics *K = V->dcast_PKinematics();
		CInifile *ini = K->LL_UserData();

		Msg("%s:%d", __FUNCTION__, __LINE__);
		{
			bone_map.clear();
			auto i = K->LL_Bones()->begin();
			auto e = K->LL_Bones()->end();
			for (; i != e; ++i)
			{
				if (bone_map.find(i->second) == bone_map.end())
				{
					bone_map.insert(std::make_pair(i->second, physicsBone()));
				}
			}
		}
		m_pPhysicsShell = P_build_Shell(this, false, &bone_map);
		m_pPhysicsShell->SetPrefereExactIntegration();
		ApplySpawnIniToPhysicShell(&D->spawn_ini(), m_pPhysicsShell, false);
		ApplySpawnIniToPhysicShell(ini, m_pPhysicsShell, false);

		if (ini->section_exist("air_resistance"))
		{
			m_pPhysicsShell->SetAirResistance(default_k_l * ini->r_float("air_resistance", "linear_factor"), default_k_w * ini->r_float("air_resistance", "angular_factor"));
		}
		m_pPhysicsShell->set_DynamicScales(1.f, 1.f);
		m_pPhysicsShell->EnabledCallbacks(TRUE);

		SAllDDOParams disable_params;
		disable_params.Load(ini);
		m_pPhysicsShell->set_DisableParams(disable_params);

		K->CalculateBones_Invalidate();
		K->CalculateBones(TRUE);
		m_pPhysicsShell->Activate();
		m_pPhysicsShell->Enable();
		CPHUpdateObject::Activate();

		//PPhysicsShell()->set_ObjectContactCallback(CollisionCallbackEnable);
		//PPhysicsShell()->set_ContactCallback(ContactCallbackAlife);
		return;
	}
#endif

	PPhysicsShell() = P_build_Shell(this, false);
	if (g_Alive())
	{
		PPhysicsShell()->EnabledCallbacks(FALSE);
		PPhysicsShell()->set_ObjectContactCallback(CollisionCallbackAlife);
		PPhysicsShell()->set_ContactCallback(ContactCallbackAlife);
		PPhysicsShell()->Disable();
	}
}

void CHelicopter::net_Save(NET_Packet& P)
{
	inherited::net_Save(P);
	CPHSkeleton::SaveNetState(P);
}

float GetCurrAcc(float V0, float V1, float dist, float a0, float a1);

void CHelicopter::MoveStep()
{
	Fvector dir, pathDir;
	float desired_H = m_movement.currPathH;
	float desired_P;
	if (m_movement.type != eMovNone)
	{
		float dist = m_movement.currP.distance_to(m_movement.desiredPoint);

		dir.sub(m_movement.desiredPoint, m_movement.currP);
		dir.normalize_safe();
		pathDir = dir;
		dir.getHP(desired_H, desired_P);
		float speed_ = _min(m_movement.GetSpeedInDestPoint(), GetMaxVelocity());

		static float ang = pSettings->r_float(cNameSect(), "magic_angle");
		if (m_movement.curLinearSpeed > GetMaxVelocity() || angle_difference(m_movement.currPathH, desired_H) > ang)
			m_movement.curLinearAcc = -m_movement.LinearAcc_bk;
		else
			m_movement.curLinearAcc = GetCurrAcc(m_movement.curLinearSpeed,
			                                     speed_,
			                                     dist * 0.95f,
			                                     m_movement.LinearAcc_fw,
			                                     -m_movement.LinearAcc_bk);


#ifdef CHELICOPTER_CHANGE
		if (fis_zero(m_movement.curLinearSpeed, EPS))
		{
			/* Snap instantly to desired direction from a stationary state. */
			m_movement.currPathH = desired_H;
			m_movement.currPathP = desired_P;
		}
		else
		{
			angle_lerp(m_movement.currPathH, desired_H, m_movement.GetAngSpeedHeading(m_movement.curLinearSpeed), STEP);
			angle_lerp(m_movement.currPathP, desired_P, m_movement.GetAngSpeedPitch(m_movement.curLinearSpeed), STEP);
		}
#else
		angle_lerp(m_movement.currPathH, desired_H, m_movement.GetAngSpeedHeading(m_movement.curLinearSpeed), STEP);
		angle_lerp(m_movement.currPathP, desired_P, m_movement.GetAngSpeedPitch(m_movement.curLinearSpeed), STEP);
#endif

		dir.setHP(m_movement.currPathH, m_movement.currPathP);

		float vp = m_movement.curLinearSpeed * STEP + (m_movement.curLinearAcc * STEP * STEP) / 2.0f;
		m_movement.currP.mad(dir, vp);
		m_movement.curLinearSpeed += m_movement.curLinearAcc * STEP;
		static bool aaa = false;
		if (aaa)
			Log("1-m_movement.curLinearSpeed=", m_movement.curLinearSpeed);
		clamp(m_movement.curLinearSpeed, 0.0f, 1000.0f);
		if (aaa)
			Log("2-m_movement.curLinearSpeed=", m_movement.curLinearSpeed);
	}
	else
	{
		//go stopping
		if (!fis_zero(m_movement.curLinearSpeed))
		{
			m_movement.curLinearAcc = -m_movement.LinearAcc_bk;

			float vp = m_movement.curLinearSpeed * STEP + (m_movement.curLinearAcc * STEP * STEP) / 2.0f;
			dir.setHP(m_movement.currPathH, m_movement.currPathP);
			dir.normalize_safe();
			m_movement.currP.mad(dir, vp);
			m_movement.curLinearSpeed += m_movement.curLinearAcc * STEP;
			clamp(m_movement.curLinearSpeed, 0.0f, 1000.0f);
			//			clamp(m_movement.curLinearSpeed,0.0f,m_movement.maxLinearSpeed);
		}
		else
		{
			m_movement.curLinearAcc = 0.0f;
			m_movement.curLinearSpeed = 0.0f;
		}
	};

	if (m_body.b_looking_at_point)
	{
		Fvector desired_dir;
		desired_dir.sub(m_body.looking_point, m_movement.currP).normalize_safe();

		float center_desired_H, tmp_P;
		desired_dir.getHP(center_desired_H, tmp_P);
		angle_lerp(m_body.currBodyHPB.x, center_desired_H, m_movement.GetAngSpeedHeading(m_movement.curLinearSpeed),
		           STEP);
	}
	else
	{
		angle_lerp(m_body.currBodyHPB.x, m_movement.currPathH, m_movement.GetAngSpeedHeading(m_movement.curLinearSpeed),
		           STEP);
	}


	float needBodyP = -m_body.model_pitch_k * m_movement.curLinearSpeed;
	if (m_movement.curLinearAcc < 0) needBodyP *= -1;
	angle_lerp(m_body.currBodyHPB.y, needBodyP, m_body.model_angSpeedPitch, STEP);


	float sign;
	Fvector cp;
	cp.crossproduct(pathDir, dir);
	(cp.y > 0.0) ? sign = 1.0f : sign = -1.0f;
	float ang_diff = angle_difference(m_movement.currPathH, desired_H);

	float needBodyB = -ang_diff * sign * m_body.model_bank_k * m_movement.curLinearSpeed;
	angle_lerp(m_body.currBodyHPB.z, needBodyB, m_body.model_angSpeedBank, STEP);


	XFORM().setHPB(m_body.currBodyHPB.x, m_body.currBodyHPB.y, m_body.currBodyHPB.z);

	XFORM().translate_over(m_movement.currP);
}

void CHelicopter::UpdateCL()
{
	inherited::UpdateCL();
	CExplosive::UpdateCL();

#ifdef CHELICOPTER_CHANGE
	CSE_Abstract *E = smart_cast<CSE_Abstract *>(ai().alife().objects().object(ID()));
	if (E)
	{
		Fvector ang;
		XFORM().getXYZ(ang);
		E->o_Angle.set(ang);
		E->o_Position.set(Position());
	}

	if (m_heli_type == eHeliTypePhysic)
	{
		if (OwnerActor() == NULL)
		{
			VisualUpdate();
		}
		return;
	}
#endif

	if (PPhysicsShell() && (state() == CHelicopter::eDead))
	{
		PPhysicsShell()->InterpolateGlobalTransform(&XFORM());

		IKinematics* K = smart_cast<IKinematics*>(Visual());
		K->CalculateBones();
		//smoke
		UpdateHeliParticles();

		if (m_brokenSound._feedback())
			m_brokenSound.set_position(XFORM().c);


			return;
	}
	else
		PPhysicsShell()->SetTransform(XFORM(), mh_unspecified);

	m_movement.Update();

	m_stepRemains += Device.fTimeDelta;
	while (m_stepRemains > STEP)
	{
		MoveStep();
		m_stepRemains -= STEP;
	}

#ifdef DEBUG
	if(bDebug){
		CGameFont* F		= UI().Font().pFontDI;
		F->SetAligment		(CGameFont::alCenter);
//		F->SetSizeI			(0.02f);
		F->OutSetI			(0.f,-0.8f);
		F->SetColor			(0xffffffff);
		F->OutNext			("Heli: speed=%4.4f acc=%4.4f dist=%4.4f",m_movement.curLinearSpeed, m_movement.curLinearAcc, m_movement.GetDistanceToDestPosition());
	}
#endif

	if (m_engineSound._feedback())
		m_engineSound.set_position(XFORM().c);


	m_enemy.Update();
	//weapon
	UpdateWeapons();
	UpdateHeliParticles();

	IKinematics* K = smart_cast<IKinematics*>(Visual());
	K->CalculateBones();
}

void CHelicopter::shedule_Update(u32 time_delta)
{
	if (!getEnabled()) return;

	inherited::shedule_Update(time_delta);
	if (CPHDestroyable::Destroyed())CPHDestroyable::SheduleUpdate(time_delta);
	else CPHSkeleton::Update(time_delta);

	if (state() != CHelicopter::eDead)
	{
		for (u32 i = getRocketCount(); i < 4; ++i)
			CRocketLauncher::SpawnRocket(*m_sRocketSection, this);
	}
	if (m_ready_explode)ExplodeHelicopter();
}

void CHelicopter::goPatrolByPatrolPath(LPCSTR path_name, int start_idx)
{
	m_movement.goPatrolByPatrolPath(path_name, start_idx);
}


void CHelicopter::goByRoundPath(Fvector center, float radius, bool clockwise)
{
	m_movement.goByRoundPath(center, radius, clockwise);
}

void CHelicopter::LookAtPoint(Fvector point, bool do_it)
{
	m_body.LookAtPoint(point, do_it);
}

void CHelicopter::save(NET_Packet& output_packet)
{
	m_movement.save(output_packet);
	m_body.save(output_packet);
	m_enemy.save(output_packet);
	output_packet.w_vec3(XFORM().c);
	output_packet.w_float(m_barrel_dir_tolerance);
	save_data(m_use_rocket_on_attack, output_packet);
	save_data(m_use_mgun_on_attack, output_packet);
	save_data(m_min_rocket_dist, output_packet);
	save_data(m_max_rocket_dist, output_packet);
	save_data(m_min_mgun_dist, output_packet);
	save_data(m_max_mgun_dist, output_packet);
	save_data(m_time_between_rocket_attack, output_packet);
	save_data(m_syncronize_rocket, output_packet);
}

void CHelicopter::load(IReader& input_packet)
{
	m_movement.load(input_packet);
	m_body.load(input_packet);
	m_enemy.load(input_packet);
#ifdef CHELICOPTER_CHANGE
	/* Shouldn't save position. Not taking teleport to account. */
	Fvector dummy;
	input_packet.r_fvector3(dummy);
#else
	input_packet.r_fvector3(XFORM().c);
#endif
	m_barrel_dir_tolerance = input_packet.r_float();
	UseFireTrail(m_enemy.bUseFireTrail); //force reloar disp params


	load_data(m_use_rocket_on_attack, input_packet);
	load_data(m_use_mgun_on_attack, input_packet);
	load_data(m_min_rocket_dist, input_packet);
	load_data(m_max_rocket_dist, input_packet);
	load_data(m_min_mgun_dist, input_packet);
	load_data(m_max_mgun_dist, input_packet);
	load_data(m_time_between_rocket_attack, input_packet);
	load_data(m_syncronize_rocket, input_packet);
}

void CHelicopter::net_Relcase(CObject* O)
{
	CExplosive::net_Relcase(O);
	inherited::net_Relcase(O);
}

/*----------------------------------------------------------------------------------------------------
	CHELICOPTER_CHANGE
----------------------------------------------------------------------------------------------------*/
#ifdef CHELICOPTER_CHANGE
/*--------------------------------------------------
	CHolderCustom
--------------------------------------------------*/
bool CHelicopter::Use(const Fvector &pos, const Fvector &dir, const Fvector &foot_pos)
{
	if (m_on_before_use_callback && strlen(m_on_before_use_callback))
	{
		luabind::functor<bool> lua_function;
		if (ai().script_engine().functor(m_on_before_use_callback, lua_function))
		{
			if (!lua_function(lua_game_object(), pos, dir, foot_pos))
			{
				return false;
			}
		}
	}
	return true;
}

bool CHelicopter::attach_Actor(CGameObject *actor)
{
	if (Owner() || CPHDestroyable::Destroyed() || CExplosive::IsExploded())
	{
		return false;
	}

	if (IsDrone())
	{
		CHolderCustom::attach_Actor(actor);
		if (OwnerActor())
		{
			if (OwnerActor()->active_cam() == eacFirstEye)
			{
				OnCameraChange(ectFirst);
			}
			else
			{
				OnCameraChange(ectChase);
			}
			DroneResetControl();
		}
		return true;
	}

	m_zoom_status = false;
	OnCameraChange(ectFirst);

	return false;
}

void CHelicopter::detach_Actor()
{
	if (!Owner())
	{
		return;
	}

	if (IsDrone())
	{
		DroneResetControl();
	}

	if (OwnerActor())
	{
		OwnerActor()->setVisible(1);
	}

	m_zoom_status = false;
	CHolderCustom::detach_Actor();
}

Fvector CHelicopter::ExitPosition()
{
	if (Owner())
	{
		Fvector().set(Owner()->Position());
	}
	return Fvector().set(0.0F, 0.0F, 0.0F);
}

Fvector CHelicopter::ExitVelocity()
{
	return Fvector().set(0.0F, 0.0F, 0.0F);
}

void CHelicopter::OnCameraChange(int type)
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

	active_camera = camera[type];
}

void CHelicopter::VisualUpdate()
{
	/* Only update m_pPhysicsShell transform here. */
	if (m_pPhysicsShell)
	{
		m_pPhysicsShell->InterpolateGlobalTransform(&XFORM());
		Visual()->dcast_PKinematics()->CalculateBones();
	}

	if (state() == CHelicopter::eDead)
	{
		if (m_brokenSound._feedback())
			m_brokenSound.set_position(XFORM().c);
	}
	else
	{
		if (m_engineSound._feedback())
			m_engineSound.set_position(XFORM().c);

		m_enemy.Update();
		UpdateWeapons();
	}
	UpdateHeliParticles();

#if 0
	if (Owner())
	{
		/* Update owner position here if needed. */
	}
#endif
}

void CHelicopter::UpdateEx(float fov)
{
	/*
		This function run by actor UpdateCL(). So don't run this in its own UpdateCL() when actor is owner.
		And update visual before update camera.
	*/
	VisualUpdate();
	if (OwnerActor() && OwnerActor()->IsMyCamera())
	{
		cam_Update(Device.fTimeDelta, fov);
		OwnerActor()->Cameras().UpdateFromCamera(Camera());
		if (eacFirstEye == active_camera->tag && !Level().Cameras().GetCamEffector(cefDemo))
		{
			OwnerActor()->Cameras().ApplyDevice(VIEWPORT_NEAR);
		}
	}
}

void CHelicopter::cam_Update(float dt, float fov)
{
	Fvector P;
	Fvector D;
	D.set(0, 0, 0);
	CCameraBase *cam = Camera();

	switch (cam->tag)
	{
	case ectFirst:
	case ectChase:
	{
		u16 bone_id = (IsCameraZoom() && (m_camera_bone_aim != BI_NONE)) ? m_camera_bone_aim : m_camera_bone_def;
		Fmatrix xform = Visual()->dcast_PKinematics()->LL_GetTransform(bone_id);
		XFORM().transform_tiny(P, xform.c);

		if (OwnerActor())
			OwnerActor()->Orientation().yaw = -cam->yaw;
		if (OwnerActor())
			OwnerActor()->Orientation().pitch = -cam->pitch;
		break;
	}
	}

	float zoom_factor = (IsCameraZoom()) ? m_zoom_factor_aim : m_zoom_factor_def;
	cam->f_fov = fov / zoom_factor;
	cam->Update(P, D);
	Level().Cameras().UpdateFromCamera(cam);
}

bool CHelicopter::SetEngineOn(bool val)
{
	if (m_on_before_engine_callback && strlen(m_on_before_engine_callback))
	{
		luabind::functor<bool> lua_function;
		if (ai().script_engine().functor(m_on_before_engine_callback, lua_function))
		{
			if (!lua_function(lua_game_object(), m_engine_on))
			{
				return false;
			}
		}
	}

	if (m_engine_on == false)
	{
		if (state() != CHelicopter::eAlive)
		{
			return false;
		}
	}

	m_engine_on = val;

	TurnEngineSound(m_engine_on);
	return true;
}

/*--------------------------------------------------
	IC
--------------------------------------------------*/
void CHelicopter::OnMouseMove(int dx, int dy)
{
	if (Remote())
		return;

	CCameraBase *cam = Camera();
	float scale = (cam->f_fov / g_fov) * psMouseSens * psMouseSensScale / 50.f;
	if (dx)
	{
		float d = float(dx) * scale;
		cam->Move((d < 0) ? kLEFT : kRIGHT, _abs(d));
	}
	if (dy)
	{
		float d = ((psMouseInvert.test(1)) ? -1 : 1) * float(dy) * scale * 3.f / 4.f;
		cam->Move((d > 0) ? kUP : kDOWN, _abs(d));
	}
}

void CHelicopter::OnKeyboardPress(int dik)
{
	if (Remote())
		return;

	if (IsDrone())
	{
		switch (dik)
		{
		/* Movement. */
		case kWPN_FIRE:
			m_control_ele = (m_control_ele == eControlEle_DW) ? eControlEle_NA : eControlEle_UP;
			break;
		case kWPN_ZOOM:
			m_control_ele = (m_control_ele == eControlEle_UP) ? eControlEle_NA : eControlEle_DW;
			break;
		case kL_LOOKOUT:
			m_control_yaw = (m_control_yaw == eControlYaw_RS) ? eControlYaw_NA : eControlYaw_LS;
			break;
		case kR_LOOKOUT:
			m_control_yaw = (m_control_yaw == eControlYaw_LS) ? eControlYaw_NA : eControlYaw_RS;
			break;
		case kFWD:
			m_control_pit = (m_control_pit == eControlPit_BS) ? eControlPit_NA : eControlPit_FS;
			break;
		case kBACK:
			m_control_pit = (m_control_pit == eControlPit_FS) ? eControlPit_NA : eControlPit_BS;
			break;
		case kL_STRAFE:
			m_control_rol = (m_control_rol == eControlRol_LS) ? eControlRol_NA : eControlRol_RS;
			break;
		case kR_STRAFE:
			m_control_rol = (m_control_rol == eControlRol_RS) ? eControlRol_NA : eControlRol_LS;
			break;
		/* Action. */
		case kJUMP:
			break;
		case kWPN_ZOOM_INC:
			m_zoom_status = true;
			break;
		case kWPN_ZOOM_DEC:
			m_zoom_status = false;
			break;
		};
	}
}

void CHelicopter::OnKeyboardRelease(int dik)
{
	if (Remote())
		return;

	if (IsDrone())
	{
		switch (dik)
		{
		/* Movement. */
		case kWPN_FIRE:
			m_control_ele = (m_control_ele == eControlEle_UP) ? eControlEle_NA : eControlEle_DW;
			break;
		case kWPN_ZOOM:
			m_control_ele = (m_control_ele == eControlEle_DW) ? eControlEle_NA : eControlEle_UP;
			break;
		case kL_LOOKOUT:
			m_control_yaw = (m_control_yaw == eControlYaw_LS) ? eControlYaw_NA : eControlYaw_RS;
			break;
		case kR_LOOKOUT:
			m_control_yaw = (m_control_yaw == eControlYaw_RS) ? eControlYaw_NA : eControlYaw_LS;
			break;
		case kFWD:
			m_control_pit = (m_control_pit == eControlPit_FS) ? eControlPit_NA : eControlPit_BS;
			break;
		case kBACK:
			m_control_pit = (m_control_pit == eControlPit_BS) ? eControlPit_NA : eControlPit_FS;
			break;
		case kL_STRAFE:
			m_control_rol = (m_control_rol == eControlRol_RS) ? eControlRol_NA : eControlRol_LS;
			break;
		case kR_STRAFE:
			m_control_rol = (m_control_rol == eControlRol_LS) ? eControlRol_NA : eControlRol_RS;
			break;
		/* Action. */
		case kJUMP:
			break;
		case kDETECTOR:
			SwitchEngine();
			break;
		/* Change camera. */
		case kCAM_1:
			OnCameraChange(ectFirst);
			break;
		case kCAM_2:
			OnCameraChange(ectChase);
			break;
		};
	}
}

void CHelicopter::OnKeyboardHold(int dik)
{
}

/*--------------------------------------------------
	Physic
--------------------------------------------------*/
void CHelicopter::PhDataUpdate(float step)
{
	/* Only update physic. Don't calculate bones. */
	if (m_pPhysicsShell == NULL)
		return;

#if 0
	RotorUpdate();
#endif

	if (IsDrone() && m_engine_on)
	{
		CPhysicsElement *E = (m_body_bone != BI_NONE) ? bone_map.find(m_body_bone)->second.element : NULL;
		if (E == NULL)
			return;

		{
			{
				float force = PPhysicsShell()->getMass() * EffectiveGravity();
				Fvector vec = Fvector().set(XFORM().j);
				// Msg("m=%f g=%f | %f,%f,%f", PPhysicsShell()->getMass(), EffectiveGravity(), vec.x, vec.y, vec.z);
				E->applyForce(vec.normalize(), force);
			}

			float kp = XFORM().k.getP();
			if (kp < -EPS_L || EPS_L < kp)
			{
				float force = m_pPhysicsShell->getMass() * EffectiveGravity() * kp * 0.5 / PI_DIV_2;
				Fvector vec = Fvector().set(1.0F, 0.0F, 0.0F);
				XFORM().transform_dir(vec);
				// Msg("m=%f g=%f k=%f | %f,%f,%f", PPhysicsShell()->getMass(), EffectiveGravity(), kp, vec.x, vec.y, vec.z);
				E->applyTorque(vec.normalize(), force);
			}

			float ip = XFORM().i.getP();
			if (ip < -EPS_L || EPS_L < ip)
			{
				float force = m_pPhysicsShell->getMass() * EffectiveGravity() * ip * 0.5 / PI_DIV_2;
				Fvector vec = Fvector().set(0.0F, 0.0F, 1.0F);
				XFORM().transform_dir(vec);
				// Msg("m=%f g=%f i=%f | %f,%f,%f", PPhysicsShell()->getMass(), EffectiveGravity(), ip, vec.x, vec.y, vec.z);
				E->applyTorque(vec.normalize(), force);
			}
		}

		Fvector velocity;
		m_pPhysicsShell->get_LinearVel(velocity);
		XFORM().transform_dir(velocity);

		switch (m_control_ele)
		{
		case eControlEle_NA:
		{
			if (velocity.y < -EPS_L || EPS_L < velocity.y)
			{
				float force = -__min(velocity.y, m_control_ele_scale) * PPhysicsShell()->getMass();
				Fvector vec = Fvector().set(0.0F, 1.0F, 0.0F);
				XFORM().transform_dir(vec);
				E->applyForce(vec.normalize(), force);
			}
			break;
		}
		case eControlEle_UP:
		case eControlEle_DW:
		{
			float force = m_control_ele_scale * PPhysicsShell()->getMass();
			Fvector vec = Fvector().set(0.0F, 1.0F, 0.0F);
			XFORM().transform_dir(vec);
			E->applyForce(vec.normalize(), (m_control_ele == eControlEle_UP) ? force : -force);
			break;
		}
		}

		Fvector angular;
		E->get_AngularVel(angular);

		switch (m_control_yaw)
		{
		case eControlYaw_NA:
		{
			if (angular.y < -EPS_L || EPS_L < angular.y)
			{
				float force = -__min(angular.y, m_control_yaw_scale) * PPhysicsShell()->getMass();
				Fvector vec = Fvector().set(0.0F, 1.0F, 0.0F);
				XFORM().transform_dir(vec);
				E->applyTorque(vec.normalize(), force);
			}
			break;
		}
		case eControlYaw_RS:
		case eControlYaw_LS:
		{
			float force = m_control_yaw_scale * PPhysicsShell()->getMass();
			Fvector vec = Fvector().set(0.0F, 1.0F, 0.0F);
			XFORM().transform_dir(vec);
			E->applyTorque(vec.normalize(), (m_control_yaw == eControlYaw_RS) ? force : -force);
			break;
		}
		}

		switch (m_control_pit)
		{
		case eControlPit_NA:
		{
			if (velocity.z < -EPS_L || EPS_L < velocity.z)
			{
				float force = -__min(velocity.z, m_control_pit_scale) * PPhysicsShell()->getMass();
				Fvector vec = Fvector().set(0.0F, 0.0F, 1.0F);
				XFORM().transform_dir(vec);
				E->applyForce(vec.normalize(), force);
			}
			break;
		}
		case eControlPit_FS:
		case eControlPit_BS:
		{
			float force = m_control_pit_scale * PPhysicsShell()->getMass();
			Fvector vec = Fvector().set(0.0F, 0.0F, 1.0F);
			XFORM().transform_dir(vec);
			E->applyForce(vec.normalize(), (m_control_pit == eControlPit_FS) ? force : -force);
			break;
		}
		}

		switch (m_control_rol)
		{
		case eControlRol_NA:
		{
			if (velocity.x < -EPS_L || EPS_L < velocity.x)
			{
				float force = -__min(velocity.x, m_control_rol_scale) * PPhysicsShell()->getMass();
				Fvector vec = Fvector().set(1.0F, 0.0F, 0.0F);
				XFORM().transform_dir(vec);
				E->applyForce(vec.normalize(), force);
			}
			break;
		}
		case eControlRol_RS:
		case eControlRol_LS:
		{
			float force = m_control_rol_scale * PPhysicsShell()->getMass();
			Fvector vec = Fvector().set(1.0F, 0.0F, 0.0F);
			XFORM().transform_dir(vec);
			E->applyForce(vec.normalize(), (m_control_rol == eControlRol_RS) ? force : -force);
			break;
		}
		}
	}
}

void CHelicopter::RotorUpdate()
{
	if (m_engine_on)
	{
		auto i = m_rotor.begin();
		auto e = m_rotor.end();
		for (; i != e; ++i)
		{
			i->second->SetForceAndVelocity(m_rotor_force_max, m_rotor_speed_max);
		}
	}
	else
	{
		auto i = m_rotor.begin();
		auto e = m_rotor.end();
		for (; i != e; ++i)
		{
			i->second->SetForceAndVelocity(0.0F, 0.0F);
		}
	}
}

void CHelicopter::DroneResetControl()
{
	SetControlEle(eControlEle_NA);
	SetControlYaw(eControlYaw_NA);
	SetControlPit(eControlPit_NA);
	SetControlRol(eControlRol_NA);
}

void CHelicopter::OnBeforeExplosion()
{
	if (OwnerActor())
	{
		OwnerActor()->use_HolderEx(NULL, true);
	}
	CExplosive::OnBeforeExplosion();
}
#endif