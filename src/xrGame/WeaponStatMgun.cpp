#include "stdafx.h"
#include "WeaponStatMgun.h"
#include "../Include/xrRender/Kinematics.h"
#include "../xrphysics/PhysicsShell.h"
#include "weaponAmmo.h"
#include "object_broker.h"
#include "ai_sounds.h"
#include "actor.h"
#include "actorEffector.h"
#include "camerafirsteye.h"
#include "xr_level_controller.h"
#include "game_object_space.h"
#include "level.h"

#ifdef CWEAPONSTATMGUN_CHANGE
#include "../xrEngine/gamemtllib.h"
#include "cameralook.h"
#include "HUDManager.h"
#include "CharacterPhysicsSupport.h"

#include "actor_anim_defs.h"
#include "ai/stalker/ai_stalker.h"
#include "stalker_animation_manager.h"
#include "sight_manager.h"
#include "stalker_planner.h"
#endif

void CWeaponStatMgun::BoneCallbackX(CBoneInstance* B)
{
	CWeaponStatMgun* P = static_cast<CWeaponStatMgun*>(B->callback_param());
	Fmatrix rX;
	rX.rotateX(P->m_cur_x_rot);
	B->mTransform.mulB_43(rX);
}

void CWeaponStatMgun::BoneCallbackY(CBoneInstance* B)
{
	CWeaponStatMgun* P = static_cast<CWeaponStatMgun*>(B->callback_param());
	Fmatrix rY;
	rY.rotateY(P->m_cur_y_rot);
	B->mTransform.mulB_43(rY);
}

CWeaponStatMgun::CWeaponStatMgun()
{
	m_firing_disabled = false;
	m_Ammo = xr_new<CCartridge>();

#ifdef CWEAPONSTATMGUN_CHANGE
	camera[ectFirst] = xr_new<CCameraFirstEye>(this, CCameraBase::flRelativeLink | CCameraBase::flPositionRigid);
	camera[ectFirst]->tag = ectFirst;
	camera[ectFirst]->Load("mounted_weapon_cam");

	camera[ectChase] = xr_new<CCameraLook>(this);
	camera[ectChase]->tag = ectChase;
	camera[ectChase]->Load("car_free_cam");

	OnCameraChange(ectFirst);
#else
	camera = xr_new<CCameraFirstEye>(
		this, CCameraBase::flRelativeLink | CCameraBase::flPositionRigid | CCameraBase::flDirectionRigid);
	camera->Load("mounted_weapon_cam");
#endif

	p_overheat = NULL;

#ifdef CWEAPONSTATMGUN_CHANGE
	m_min_gun_speed = 0.0F;
	m_max_gun_speed = 0.0F;
	m_turn_default = true;

	m_actor_bone = BI_NONE;
	m_exit_bone = BI_NONE;
	m_exit_position.set(0, 0, 0);
	m_user_position.set(0, 0, 0);

	m_camera_bone_def = BI_NONE;
	m_camera_bone_aim = BI_NONE;
	m_zoom_factor_def = 1.0F;
	m_zoom_factor_aim = 1.0F;
	m_zoom_status = false;
	m_camera_position.set(0, 0, 0);

	m_animation = "norm_torso_m134_aim_0";

	m_on_before_use_callback = NULL;
#endif
}

CWeaponStatMgun::~CWeaponStatMgun()
{
	delete_data(m_Ammo);
#ifdef CWEAPONSTATMGUN_CHANGE
	xr_delete(camera[ectFirst]);
	xr_delete(camera[ectChase]);
#else
	xr_delete(camera);
#endif
}

void CWeaponStatMgun::SetBoneCallbacks()
{
	m_pPhysicsShell->EnabledCallbacks(FALSE);

	CBoneInstance& biX = smart_cast<IKinematics*>(Visual())->LL_GetBoneInstance(m_rotate_x_bone);
	biX.set_callback(bctCustom, BoneCallbackX, this);
	CBoneInstance& biY = smart_cast<IKinematics*>(Visual())->LL_GetBoneInstance(m_rotate_y_bone);
	biY.set_callback(bctCustom, BoneCallbackY, this);
}

void CWeaponStatMgun::ResetBoneCallbacks()
{
	CBoneInstance& biX = smart_cast<IKinematics*>(Visual())->LL_GetBoneInstance(m_rotate_x_bone);
	biX.reset_callback();
	CBoneInstance& biY = smart_cast<IKinematics*>(Visual())->LL_GetBoneInstance(m_rotate_y_bone);
	biY.reset_callback();

	m_pPhysicsShell->EnabledCallbacks(TRUE);
}

void CWeaponStatMgun::Load(LPCSTR section)
{
	inheritedPH::Load(section);
	inheritedShooting::Load(section);

	m_sounds.LoadSound(section, "snd_shoot", "sndShot", false, SOUND_TYPE_WEAPON_SHOOTING);

	m_Ammo->Load(pSettings->r_string(section, "ammo_class"), 0);
	camMaxAngle = pSettings->r_float(section, "cam_max_angle");
	camMaxAngle = _abs(deg2rad(camMaxAngle));
	camRelaxSpeed = pSettings->r_float(section, "cam_relax_speed");
	camRelaxSpeed = _abs(deg2rad(camRelaxSpeed));

	m_overheat_enabled = pSettings->line_exist(section, "overheat_enabled")
		                     ? !!pSettings->r_bool(section, "overheat_enabled")
		                     : false;
	m_overheat_time_quant = READ_IF_EXISTS(pSettings, r_float, section, "overheat_time_quant", 0.025f);
	m_overheat_decr_quant = READ_IF_EXISTS(pSettings, r_float, section, "overheat_decr_quant", 0.002f);
	m_overheat_threshold = READ_IF_EXISTS(pSettings, r_float, section, "overheat_threshold", 110.f);
	m_overheat_particles = READ_IF_EXISTS(pSettings, r_string, section, "overheat_particles",
	                                      "damage_fx\\burn_creatures00");

	m_bEnterLocked = !!READ_IF_EXISTS(pSettings, r_bool, section, "lock_enter", false);
	m_bExitLocked = !!READ_IF_EXISTS(pSettings, r_bool, section, "lock_exit", false);

	VERIFY(!fis_zero(camMaxAngle));
	VERIFY(!fis_zero(camRelaxSpeed));

#ifdef CWEAPONSTATMGUN_CHANGE
	if (pSettings->line_exist(cNameSect_str(), "on_before_use"))
	{
		m_on_before_use_callback = READ_IF_EXISTS(pSettings, r_string, cNameSect_str(), "on_before_use", "");
	}
#endif
}

BOOL CWeaponStatMgun::net_Spawn(CSE_Abstract* DC)
{
	if (!inheritedPH::net_Spawn(DC)) return FALSE;


	IKinematics* K = smart_cast<IKinematics*>(Visual());
	CInifile* pUserData = K->LL_UserData();

	R_ASSERT2(pUserData, "Empty WeaponStatMgun user data!");

	m_rotate_x_bone = K->LL_BoneID(pUserData->r_string("mounted_weapon_definition", "rotate_x_bone"));
	m_rotate_y_bone = K->LL_BoneID(pUserData->r_string("mounted_weapon_definition", "rotate_y_bone"));
	m_fire_bone = K->LL_BoneID(pUserData->r_string("mounted_weapon_definition", "fire_bone"));
	m_camera_bone = K->LL_BoneID(pUserData->r_string("mounted_weapon_definition", "camera_bone"));

	U16Vec fixed_bones;
	fixed_bones.push_back(K->LL_GetBoneRoot());
	PPhysicsShell() = P_build_Shell(this, false, fixed_bones);

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
	m_bind_x_rot = matrices[m_rotate_x_bone].k.getP();
	m_bind_y_rot = matrices[m_rotate_y_bone].k.getH();
	m_bind_x.set(matrices[m_rotate_x_bone].c);
	m_bind_y.set(matrices[m_rotate_y_bone].c);

#ifdef CWEAPONSTATMGUN_CHANGE
	/*
	Initial directions are always H = 0 P = 0. Even when the bones were created with angled directions.
	Example: When a bone is created with H = 60 degrees, the bone has a H = 60 degrees angle to the model.
	But it is H = 0 degree to the bone itself.
	*/
	m_cur_x_rot = 0;
	m_cur_y_rot = 0;
#else
	m_cur_x_rot = m_bind_x_rot;
	m_cur_y_rot = m_bind_y_rot;
#endif

	m_destEnemyDir.setHP(m_bind_y_rot, m_bind_x_rot);
	XFORM().transform_dir(m_destEnemyDir);

#ifdef CWEAPONSTATMGUN_CHANGE
	CInifile *ini = K->LL_UserData();

	m_min_gun_speed = deg2rad(READ_IF_EXISTS(ini, r_float, "mounted_weapon_definition", "min_gun_speed", 20.0F));
	m_max_gun_speed = deg2rad(READ_IF_EXISTS(ini, r_float, "mounted_weapon_definition", "max_gun_speed", 20.0F));

	m_actor_bone = ini->line_exist("config", "actor_bone") ? K->LL_BoneID(ini->r_string("config", "actor_bone")) : BI_NONE;
	m_exit_bone = ini->line_exist("config", "exit_bone") ? K->LL_BoneID(ini->r_string("config", "exit_bone")) : BI_NONE;
	m_exit_position = READ_IF_EXISTS(ini, r_fvector3, "config", "exit_position", Fvector().set(0, 0, 0));
	m_user_position = READ_IF_EXISTS(ini, r_fvector3, "config", "user_position", Fvector().set(0, 0, 0));

	m_camera_bone_def = ini->line_exist("config", "camera_bone_def") ? K->LL_BoneID(ini->r_string("config", "camera_bone_def")) : BI_NONE;
	m_camera_bone_aim = ini->line_exist("config", "camera_bone_aim") ? K->LL_BoneID(ini->r_string("config", "camera_bone_aim")) : BI_NONE;
	m_zoom_factor_def = READ_IF_EXISTS(ini, r_float, "config", "zoom_factor_def", 1.0F);
	m_zoom_factor_aim = READ_IF_EXISTS(ini, r_float, "config", "zoom_factor_aim", 1.0F);
	m_camera_position = READ_IF_EXISTS(ini, r_fvector3, "config", "camera_position", Fvector().set(0, 0, 0));

	m_animation = READ_IF_EXISTS(ini, r_string, "config", "animation", "norm_torso_m134_aim_0");

	if (ini->line_exist("camera", "camera_first") && pSettings->section_exist(ini->r_string("camera", "camera_first")))
	{
		camera[ectFirst]->Load(ini->r_string("camera", "camera_first"));
	}
	if (ini->line_exist("camera", "camera_chase") && pSettings->section_exist(ini->r_string("camera", "camera_chase")))
	{
		camera[ectChase]->Load(ini->r_string("camera", "camera_chase"));
	}
#endif

	inheritedShooting::Light_Create();

	processing_activate();
	setVisible(TRUE);
	setEnabled(TRUE);

#ifdef CWEAPONSTATMGUN_CHANGE
	PPhysicsShell()->Enable();
	PPhysicsShell()->add_ObjectContactCallback(IgnoreOwnerCallback);
	SetBoneCallbacks();
#endif

	return TRUE;
}

void CWeaponStatMgun::net_Destroy()
{
	if (p_overheat)
	{
		if (p_overheat->IsPlaying())
			p_overheat->Stop(FALSE);
		CParticlesObject::Destroy(p_overheat);
	}

#ifdef CWEAPONSTATMGUN_CHANGE
#ifdef CHOLDERCUSTOM_CHANGE
	if (Owner()->cast_stalker() && !Owner()->cast_stalker()->g_Alive())
	{
		Owner()->cast_stalker()->detach_Holder();
		return;
	}
#endif

	if (OwnerActor() && !OwnerActor()->g_Alive())
	{
		OwnerActor()->use_HolderEx(NULL, true);
		return;
	}

	PPhysicsShell()->remove_ObjectContactCallback(IgnoreOwnerCallback);
	ResetBoneCallbacks();
#endif

	inheritedPH::net_Destroy();
	processing_deactivate();
}

void CWeaponStatMgun::net_Export(NET_Packet& P) // export to server
{
	inheritedPH::net_Export(P);
	P.w_u8(IsWorking() ? 1 : 0);
	save_data(m_destEnemyDir, P);
}

void CWeaponStatMgun::net_Import(NET_Packet& P) // import from server
{
	inheritedPH::net_Import(P);
	u8 state = P.r_u8();
	load_data(m_destEnemyDir, P);

	if (TRUE == IsWorking() && !state) FireEnd();
	if (FALSE == IsWorking() && state) FireStart();
}

void CWeaponStatMgun::UpdateCL()
{
	inheritedPH::UpdateCL();

#ifdef CWEAPONSTATMGUN_CHANGE
	if (Owner())
	{
		UpdateOwner();
	}

	UpdateBarrelDir();
	UpdateFire();
#else
	UpdateBarrelDir();
	UpdateFire();

	if (OwnerActor() && OwnerActor()->IsMyCamera())
	{
		cam_Update(Device.fTimeDelta, g_fov);
		OwnerActor()->Cameras().UpdateFromCamera(Camera());
		OwnerActor()->Cameras().ApplyDevice(VIEWPORT_NEAR);
	}
#endif
}

//void CWeaponStatMgun::Hit(	float P, Fvector &dir,	CObject* who, 
//							s16 element,Fvector p_in_object_space, 
//							float impulse, ALife::EHitType hit_type)
void CWeaponStatMgun::Hit(SHit* pHDS)
{
#ifdef CWEAPONSTATMGUN_CHANGE
	inheritedPH::Hit(pHDS);
#else
	if (NULL == Owner())
		//		inheritedPH::Hit(P,dir,who,element,p_in_object_space,impulse,hit_type);
		inheritedPH::Hit(pHDS);
#endif
}

void CWeaponStatMgun::UpdateBarrelDir()
{
	IKinematics* K = smart_cast<IKinematics*>(Visual());
	m_fire_bone_xform = K->LL_GetTransform(m_fire_bone);

	m_fire_bone_xform.mulA_43(XFORM());
	m_fire_pos.set(0, 0, 0);
	m_fire_bone_xform.transform_tiny(m_fire_pos);
	m_fire_dir.set(0, 0, 1);
	m_fire_bone_xform.transform_dir(m_fire_dir);

	m_allow_fire = true;
	Fmatrix XFi;
	XFi.invert(XFORM());
	Fvector dep;

#ifdef CWEAPONSTATMGUN_CHANGE
	/*
		Take m_i_bind_y_xform as base for both bone x and bone y. Assume bone y will always have 0 pitch.
		And reset dep before calculating.
	*/
	{
		// x angle
		if (m_turn_default)
		{
			dep.setHP(m_bind_y_rot, m_bind_x_rot);
		}
		else
		{
			XFi.transform_dir(dep, m_destEnemyDir);
		}
		m_i_bind_y_xform.transform_dir(dep);
		dep.normalize();
		m_tgt_x_rot = angle_normalize_signed(m_bind_x_rot - dep.getP());
		clamp(m_tgt_x_rot, -m_lim_x_rot.y, -m_lim_x_rot.x);
	}
	{
		// y angle
		if (m_turn_default)
		{
			dep.setHP(m_bind_y_rot, m_bind_x_rot);
		}
		else
		{
			XFi.transform_dir(dep, m_destEnemyDir);
		}
		m_i_bind_y_xform.transform_dir(dep);
		dep.normalize();
		m_tgt_y_rot = angle_normalize_signed(m_bind_y_rot - dep.getH());
		clamp(m_tgt_y_rot, -m_lim_y_rot.y, -m_lim_y_rot.x);
	}

	m_cur_x_rot = angle_inertion_var(m_cur_x_rot, m_tgt_x_rot, m_min_gun_speed, m_max_gun_speed, PI, Device.fTimeDelta);
	m_cur_y_rot = angle_inertion_var(m_cur_y_rot, m_tgt_y_rot, m_min_gun_speed, m_max_gun_speed, PI, Device.fTimeDelta);
	static float dir_eps = deg2rad(5.0f);
	if (!fsimilar(m_cur_x_rot, m_tgt_x_rot, dir_eps) || !fsimilar(m_cur_y_rot, m_tgt_y_rot, dir_eps))
	{
		m_allow_fire = FALSE;
	}
#else
	XFi.transform_dir(dep, m_destEnemyDir);
	{
		// x angle
		m_i_bind_x_xform.transform_dir(dep);
		dep.normalize();
		m_tgt_x_rot = angle_normalize_signed(m_bind_x_rot - dep.getP());
		float sv_x = m_tgt_x_rot;

		clamp(m_tgt_x_rot, -m_lim_x_rot.y, -m_lim_x_rot.x);
		if (!fsimilar(sv_x, m_tgt_x_rot, EPS_L)) m_allow_fire = FALSE;
	}
	{
		// y angle
		m_i_bind_y_xform.transform_dir(dep);
		dep.normalize();
		m_tgt_y_rot = angle_normalize_signed(m_bind_y_rot - dep.getH());
		float sv_y = m_tgt_y_rot;
		clamp(m_tgt_y_rot, -m_lim_y_rot.y, -m_lim_y_rot.x);
		if (!fsimilar(sv_y, m_tgt_y_rot, EPS_L)) m_allow_fire = FALSE;
	}

	m_cur_x_rot = angle_inertion_var(m_cur_x_rot, m_tgt_x_rot, 0.5f, 3.5f, PI_DIV_6, Device.fTimeDelta);
	m_cur_y_rot = angle_inertion_var(m_cur_y_rot, m_tgt_y_rot, 0.5f, 3.5f, PI_DIV_6, Device.fTimeDelta);
#endif
}

void CWeaponStatMgun::cam_Update(float dt, float fov)
{
#ifdef CWEAPONSTATMGUN_CHANGE
	Fvector P;
	Fvector D;
	D.set(0, 0, 0);
	CCameraBase *cam = Camera();

	switch (cam->tag)
	{
	case ectFirst:
	case ectChase:
	{
		u16 bone_id = m_camera_bone;
		if (m_camera_bone_def != BI_NONE)
		{
			bone_id = (IsCameraZoom() && (m_camera_bone_aim != BI_NONE)) ? m_camera_bone_aim : m_camera_bone_def;
		}

		IKinematics *K = Visual()->dcast_PKinematics();
		Fmatrix xform = K->LL_GetTransform(bone_id);
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
#else
	camera->f_fov = g_fov;

	Fvector P, Da;
	Da.set(0, 0, 0);

	IKinematics* K = smart_cast<IKinematics*>(Visual());
	K->CalculateBones_Invalidate();
	K->CalculateBones(TRUE);
	const Fmatrix& C = K->LL_GetTransform(m_camera_bone);
	XFORM().transform_tiny(P, C.c);

	Fvector d = C.k;
	XFORM().transform_dir(d);
	Fvector2 des_cam_dir;

	d.getHP(des_cam_dir.x, des_cam_dir.y);
	des_cam_dir.mul(-1.0f);


	Camera()->yaw = angle_inertion_var(Camera()->yaw, des_cam_dir.x, 0.5f, 7.5f, PI_DIV_6, Device.fTimeDelta);
	Camera()->pitch = angle_inertion_var(Camera()->pitch, des_cam_dir.y, 0.5f, 7.5f, PI_DIV_6, Device.fTimeDelta);


	if (OwnerActor())
	{
		// rotate head
		OwnerActor()->Orientation().yaw = -Camera()->yaw;
		OwnerActor()->Orientation().pitch = -Camera()->pitch;
	}


	Camera()->Update(P, Da);
	Level().Cameras().UpdateFromCamera(Camera());
#endif
}

void CWeaponStatMgun::renderable_Render()
{
	inheritedPH::renderable_Render();

	RenderLight();
}

void CWeaponStatMgun::SetDesiredDir(float h, float p)
{
	m_destEnemyDir.setHP(h, p);
#ifdef CWEAPONSTATMGUN_CHANGE
	m_turn_default = false;
#endif
}

void CWeaponStatMgun::Action(u16 id, u32 flags)
{
	inheritedHolder::Action(id, flags);
	switch (id)
	{
#ifdef CWEAPONSTATMGUN_CHANGE
	case eWpnFire:
		if (flags)
		{
			if (!IsWorking())
				FireStart();
		}
		else
		{
			if (IsWorking())
				FireEnd();
		}
		break;
	case eWpnDesiredDef:
		m_turn_default = true;
		break;
	default:
		break;
#else
	case kWPN_FIRE:
		{
			if (flags == CMD_START) FireStart();
			else FireEnd();
		}
		break;
#endif
	}
}

void CWeaponStatMgun::SetParam(int id, Fvector2 val)
{
	inheritedHolder::SetParam(id, val);
	switch (id)
	{
	case DESIRED_DIR:
		SetDesiredDir(val.x, val.y);
		break;
	}
}

#ifdef CWEAPONSTATMGUN_CHANGE
void CWeaponStatMgun::SetParam(int id, Fvector val)
{
	inheritedHolder::SetParam(id, val);
	switch (id)
	{
	case eWpnDesiredPos:
		Fvector vec = Fmatrix().mul_43(XFORM(), Visual()->dcast_PKinematics()->LL_GetTransform(m_rotate_y_bone)).c;
		m_destEnemyDir.sub(val, vec).normalize_safe();
		m_turn_default = false;
		break;
	case eWpnDesiredDir:
		m_destEnemyDir.set(val).normalize_safe();
		m_turn_default = false;
		break;
	}
}
#endif

bool CWeaponStatMgun::attach_Actor(CGameObject* actor)
{
#ifdef CWEAPONSTATMGUN_CHANGE
	if (Owner())
		return false;

	if (!actor)
		return false;

	inheritedHolder::attach_Actor(actor);
	if (OwnerActor())
	{
		if (Camera()->tag == ectFirst)
		{
			OwnerActor()->setVisible(FALSE);
		}
	}
	FireEnd();
	return true;
#else
	if (Owner())
		return false;
	actor->setVisible(0);
	inheritedHolder::attach_Actor(actor);
	SetBoneCallbacks();
	FireEnd();
	return true;
#endif
}

void CWeaponStatMgun::detach_Actor()
{
#ifdef CWEAPONSTATMGUN_CHANGE
	if (!Owner())
		return;

	if (OwnerActor())
	{
		OwnerActor()->setVisible(TRUE);
	}
	inheritedHolder::detach_Actor();
	FireEnd();
#else
	Owner()->setVisible(1);
	inheritedHolder::detach_Actor();
	ResetBoneCallbacks();
	FireEnd();
#endif
}

Fvector CWeaponStatMgun::ExitPosition()
{
#ifdef CWEAPONSTATMGUN_CHANGE
	Fvector pos;
	pos.set(0.0F, 0.0F, 0.0F);
	IKinematics *K = Visual()->dcast_PKinematics();
	Fmatrix xform = K->LL_GetTransform(m_actor_bone);
	XFORM().transform_tiny(pos, xform.c);
	return pos;
#else
	Fvector pos; pos.set(0.f, 0.f, 0.f);
	pos.sub(camera->Direction()).normalize();
	pos.y = 0.f;
	return Fvector(XFORM().c).add(pos);
#endif
}

#ifdef CWEAPONSTATMGUN_CHANGE
BOOL CWeaponStatMgun::AlwaysTheCrow()
{
	return TRUE;
}

bool CWeaponStatMgun::is_ai_obstacle() const
{
	return true;
}

void CWeaponStatMgun::IgnoreOwnerCallback(bool &do_colide, bool bo1, dContact &c, SGameMtl *mt1, SGameMtl *mt2)
{
	if (do_colide == false)
		return;

	dxGeomUserData *gd1 = bo1 ? PHRetrieveGeomUserData(c.geom.g1) : PHRetrieveGeomUserData(c.geom.g2);
	dxGeomUserData *gd2 = bo1 ? PHRetrieveGeomUserData(c.geom.g2) : PHRetrieveGeomUserData(c.geom.g1);
	CGameObject *obj = (gd1) ? smart_cast<CGameObject *>(gd1->ph_ref_object) : nullptr;
	CGameObject *who = (gd2) ? smart_cast<CGameObject *>(gd2->ph_ref_object) : nullptr;
	if (!obj || !who)
	{
		return;
	}

	CWeaponStatMgun *stm = smart_cast<CWeaponStatMgun *>(obj);
	CGameObject *owner = (stm) ? stm->Owner() : nullptr;
	if (owner && (owner->ID() == who->ID()))
	{
		do_colide = false;
	}
}

bool CWeaponStatMgun::Use(const Fvector &pos, const Fvector &dir, const Fvector &foot_pos)
{
	if (Owner())
		return false;

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

void CWeaponStatMgun::UpdateOwner()
{
#ifdef CHOLDERCUSTOM_CHANGE
	if (Owner()->cast_stalker() && !Owner()->cast_stalker()->g_Alive())
	{
		Owner()->cast_stalker()->detach_Holder();
		return;
	}
#endif

	if (OwnerActor() && !OwnerActor()->g_Alive())
	{
		OwnerActor()->use_HolderEx(NULL, true);
		return;
	}

	if (m_actor_bone != BI_NONE)
	{
		IKinematics *K = Visual()->dcast_PKinematics();
		Owner()->XFORM().set(Fmatrix().mul_43(XFORM(), K->LL_GetTransform(m_actor_bone)));
	}
	else
	{
		Owner()->XFORM().set(XFORM());
	}

	if (OwnerActor() && OwnerActor()->IsMyCamera())
	{
		cam_Update(Device.fTimeDelta, g_fov);
		OwnerActor()->Cameras().UpdateFromCamera(Camera());
		OwnerActor()->Cameras().ApplyDevice(VIEWPORT_NEAR);

		if (IsCameraZoom())
		{
			m_destEnemyDir.set(Camera()->Direction());
			m_turn_default = false;
		}
		else
		{
			Fvector pos = Camera()->Position();
			Fvector dir = Camera()->Direction();
			collide::rq_result &R = HUD().GetCurrentRayQuery();
			Fvector vec = Fvector().mad(pos, dir, (R.range > 3.f) ? R.range : 30.f);
			m_destEnemyDir.sub(vec, m_fire_pos).normalize();
			m_turn_default = false;
		}
	}
}

void CWeaponStatMgun::OnCameraChange(u16 type)
{
	if (OwnerActor())
	{
		if (type == ectFirst)
		{
			OwnerActor()->setVisible(FALSE);
		}
		else
		{
			OwnerActor()->setVisible(TRUE);
		}
	}

	if (active_camera == NULL || active_camera->tag != type)
	{
		active_camera = camera[type];
	}
}

float CWeaponStatMgun::FireDirDiff()
{
	Fvector v1 = Fvector().set(m_fire_dir).normalize_safe();
	Fvector v2 = Fvector().set(m_destEnemyDir).normalize_safe();
#if 0
	Msg("[%s] v1 %.1f %.1f", cName().c_str(), rad2deg(v1.getH()), rad2deg(v1.getP()));
	Msg("[%s] v2 %.1f %.1f", cName().c_str(), rad2deg(v2.getH()), rad2deg(v2.getP()));
#endif
	return abs(acos(v1.dotproduct(v2)));
}

bool CWeaponStatMgun::InFieldOfView(Fvector pos)
{
	Fvector vec;
	Fmatrix().invert(XFORM()).transform_tiny(vec, pos);
	Fvector dir = Fvector().sub(vec, Visual()->dcast_PKinematics()->LL_GetTransform(m_rotate_y_bone).c).normalize();
	float h = dir.getH();
	float p = dir.getP();
#if 0
	Msg("[%s] Y %.1f %.1f %.1f", cName().c_str(), rad2deg(h), rad2deg(-m_lim_y_rot.y), rad2deg(-m_lim_y_rot.x));
	Msg("[%s] X %.1f %.1f %.1f", cName().c_str(), rad2deg(p), rad2deg(-m_lim_x_rot.y), rad2deg(-m_lim_x_rot.x));
#endif
	if (h < -m_lim_y_rot.y || -m_lim_y_rot.x < h)
	{
		return false;
	}
	if (p < -m_lim_x_rot.y || -m_lim_x_rot.x < p)
	{
		return false;
	}
	return true;
}

Fvector CWeaponStatMgun::UserPosition()
{
	Fvector vec;
	XFORM().transform_tiny(vec, m_user_position);
	return vec;
}
#endif