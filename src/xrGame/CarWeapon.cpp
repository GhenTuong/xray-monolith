#include "stdafx.h"
#include "CarWeapon.h"
#include "../xrphysics/PhysicsShell.h"
#include "PhysicsShellHolder.h"
#include "../Include/xrRender/Kinematics.h"
#include "object_broker.h"
#include "ai_sounds.h"
#include "weaponAmmo.h"
#include "xr_level_controller.h"
#include "game_object_space.h"
#include "holder_custom.h"

#ifdef CHOLDERCUSTOM_CHANGE

#endif

void CCarWeapon::BoneCallbackX(CBoneInstance* B)
{
	CCarWeapon* P = static_cast<CCarWeapon*>(B->callback_param());
	Fmatrix rX;
	rX.rotateX(P->m_cur_x_rot);
	B->mTransform.mulB_43(rX);
}

void CCarWeapon::BoneCallbackY(CBoneInstance* B)
{
	CCarWeapon* P = static_cast<CCarWeapon*>(B->callback_param());
	Fmatrix rY;
	rY.rotateY(P->m_cur_y_rot);
	B->mTransform.mulB_43(rY);
}

CCarWeapon::CCarWeapon(CPhysicsShellHolder* obj)
{
	m_bActive = false;
	m_bAutoFire = false;
	m_object = obj;
	m_Ammo = xr_new<CCartridge>();

	IKinematics* K = smart_cast<IKinematics*>(m_object->Visual());
	CInifile* pUserData = K->LL_UserData();

	m_rotate_x_bone = K->LL_BoneID(pUserData->r_string("mounted_weapon_definition", "rotate_x_bone"));
	m_rotate_y_bone = K->LL_BoneID(pUserData->r_string("mounted_weapon_definition", "rotate_y_bone"));
	m_fire_bone = K->LL_BoneID(pUserData->r_string("mounted_weapon_definition", "fire_bone"));
	m_min_gun_speed = pUserData->r_float("mounted_weapon_definition", "min_gun_speed");
	m_max_gun_speed = pUserData->r_float("mounted_weapon_definition", "max_gun_speed");
	CBoneData& bdX = K->LL_GetData(m_rotate_x_bone); //VERIFY(bdX.IK_data.type==jtJoint);
	m_lim_x_rot.set(bdX.IK_data.limits[0].limit.x, bdX.IK_data.limits[0].limit.y);
	CBoneData& bdY = K->LL_GetData(m_rotate_y_bone); //VERIFY(bdY.IK_data.type==jtJoint);
	m_lim_y_rot.set(bdY.IK_data.limits[1].limit.x, bdY.IK_data.limits[1].limit.y);

	xr_vector<Fmatrix> matrices;
	K->LL_GetBindTransform(matrices);
	m_i_bind_x_xform.invert(matrices[m_rotate_x_bone]);
	m_i_bind_y_xform.invert(matrices[m_rotate_y_bone]);
	m_bind_x_rot = matrices[m_rotate_x_bone].k.getP();
	m_bind_y_rot = matrices[m_rotate_y_bone].k.getH();
	m_bind_x.set(matrices[m_rotate_x_bone].c);
	m_bind_y.set(matrices[m_rotate_y_bone].c);

	m_cur_x_rot = m_bind_x_rot;
	m_cur_y_rot = m_bind_y_rot;
	m_destEnemyDir.setHP(m_bind_y_rot, m_bind_x_rot);
	m_object->XFORM().transform_dir(m_destEnemyDir);


	inheritedShooting::Light_Create();
	Load(pUserData->r_string("mounted_weapon_definition", "wpn_section"));

#ifdef CHOLDERCUSTOM_CHANGE
	/* Remove */
#else
	SetBoneCallbacks();
#endif

	m_object->processing_activate();

	m_weapon_h = matrices[m_rotate_y_bone].c.y;
	m_fire_norm.set(0, 1, 0);
	m_fire_dir.set(0, 0, 1);
	m_fire_pos.set(0, 0, 0);

#ifdef CHOLDERCUSTOM_CHANGE
	m_holder = smart_cast<CHolderCustom *>(m_object);
	R_ASSERT3(m_holder, "Parent is not a holder custom. ", m_object->cNameSect_str());

	m_state_index = eCarWeaponStateIdle;
	m_state_delay = 0;

	CInifile *ini = K->LL_UserData();
	LPCSTR mwd = "mounted_weapon_definition";

	m_drop_bone = ini->line_exist(mwd, "drop_bone") ? K->LL_BoneID(ini->r_string(mwd, "drop_bone")) : BI_NONE;
	m_ammo_bone = ini->line_exist(mwd, "ammo_bone") ? K->LL_BoneID(ini->r_string(mwd, "ammo_bone")) : BI_NONE;
	m_recoil_force = READ_IF_EXISTS(ini, r_float, mwd, "recoil_force", 0.0F);
	m_min_gun_speed = _abs(deg2rad(m_min_gun_speed));
	m_max_gun_speed = _abs(deg2rad(m_max_gun_speed));

	m_desire_angle_vector.set(0.0F, 0.0F);
	m_desire_angle_active = false;

	m_unlimited_ammo = !!READ_IF_EXISTS(pSettings, r_bool, m_object->cNameSect_str(), "unlimited_ammo", true);
	m_reload_consume_inventory_callback = READ_IF_EXISTS(pSettings, r_string, m_object->cNameSect_str(), "reload_consume_inventory", NULL);
#endif
}

CCarWeapon::~CCarWeapon()
{
	delete_data(m_Ammo);
	//.	m_object->processing_deactivate		();
}

void CCarWeapon::Load(LPCSTR section)
{
	inheritedShooting::Load(section);
#ifdef CHOLDERCUSTOM_CHANGE
	m_sounds.LoadSound(section, "snd_shoot", "sndShot", false, SOUND_TYPE_WEAPON_SHOOTING);
	m_sounds.LoadSound(section, "snd_reload", "sndReload", true, SOUND_TYPE_WEAPON_RECHARGING);

	m_rocket_enable = !!READ_IF_EXISTS(pSettings, r_bool, section, "is_rocket_launcher", false);
	if (m_rocket_enable)
	{
		CRocketLauncher::Load(section);
	}

	m_ammoTypes.clear();
	LPCSTR ammo_class = pSettings->r_string(section, "ammo_class");
	if (ammo_class && strlen(ammo_class))
	{
		string128 tmp;
		int n = _GetItemCount(ammo_class);
		for (int i = 0; i < n; ++i)
		{
			_GetItem(ammo_class, i, tmp);
			m_ammoTypes.push_back(tmp);
		}
	}
	m_ammoType = 0;
	m_DefaultCartridge.Load(m_ammoTypes[m_ammoType].c_str(), m_ammoType, 1.0F);

	iMagazineSize = pSettings->r_s32(section, "ammo_mag_size");
	SetAmmoElapsed(READ_IF_EXISTS(pSettings, r_s32, section, "ammo_elapsed", iMagazineSize));
#else
	HUD_SOUND_ITEM::LoadSound(section, "snd_shoot", m_sndShot, SOUND_TYPE_WEAPON_SHOOTING);
	m_Ammo->Load(pSettings->r_string(section, "ammo_class"), 0);
#endif
}

void CCarWeapon::UpdateCL()
{
	if (!m_bActive) return;

	UpdateBarrelDir();
	IKinematics* K = smart_cast<IKinematics*>(m_object->Visual());
	K->CalculateBones_Invalidate();
	K->CalculateBones(TRUE);
	UpdateFire();
}

void CCarWeapon::UpdateFire()
{
	fShotTimeCounter -= Device.fTimeDelta;

	inheritedShooting::UpdateFlameParticles();
	inheritedShooting::UpdateLight();

	if (m_bAutoFire)
	{
		if (m_allow_fire)
		{
			FireStart();
		}
		else
			FireEnd();
	};

#ifdef CHOLDERCUSTOM_CHANGE
	switch (m_state_index)
	{
	case eCarWeaponStateIdle:
	case eCarWeaponStateFire:
		break;
	case eCarWeaponStateReload:
		UpdateReload();
		if (m_state_index == eCarWeaponStateReload)
			return;
		break;
	case eCarWeaponStateUnload:
		UpdateUnload();
		if (m_state_index == eCarWeaponStateUnload)
			return;
		break;
	default:
		break;
	}
#endif

	if (!IsWorking())
	{
		clamp(fShotTimeCounter, 0.0f, flt_max);
		return;
	}

#ifdef CHOLDERCUSTOM_CHANGE
	/* Out of ammo. */
	if (!m_unlimited_ammo && (m_magazine.size() == 0))
	{
		SwitchState(eCarWeaponStateReload);
		return;
	}
#endif

	if (fShotTimeCounter <= 0)
	{
		OnShot();
		fShotTimeCounter += fOneShotTime;
	}
}

void CCarWeapon::Render_internal()
{
	RenderLight();
}

void CCarWeapon::SetBoneCallbacks()
{
	//	m_object->PPhysicsShell()->EnabledCallbacks(FALSE);

	CBoneInstance& biX = smart_cast<IKinematics*>(m_object->Visual())->LL_GetBoneInstance(m_rotate_x_bone);
	biX.set_callback(bctCustom, BoneCallbackX, this);
	CBoneInstance& biY = smart_cast<IKinematics*>(m_object->Visual())->LL_GetBoneInstance(m_rotate_y_bone);
	biY.set_callback(bctCustom, BoneCallbackY, this);
}

void CCarWeapon::ResetBoneCallbacks()
{
	CBoneInstance& biX = smart_cast<IKinematics*>(m_object->Visual())->LL_GetBoneInstance(m_rotate_x_bone);
	biX.reset_callback();
	CBoneInstance& biY = smart_cast<IKinematics*>(m_object->Visual())->LL_GetBoneInstance(m_rotate_y_bone);
	biY.reset_callback();

	//	m_object->PPhysicsShell()->EnabledCallbacks(TRUE);
}

void CCarWeapon::UpdateBarrelDir()
{
	IKinematics* K = smart_cast<IKinematics*>(m_object->Visual());
	m_fire_bone_xform = K->LL_GetTransform(m_fire_bone);

	m_fire_bone_xform.mulA_43(m_object->XFORM());
	m_fire_pos.set(0, 0, 0);
	m_fire_bone_xform.transform_tiny(m_fire_pos);
	m_fire_dir.set(0, 0, 1);
	m_fire_bone_xform.transform_dir(m_fire_dir);
	m_fire_norm.set(0, 1, 0);
	m_fire_bone_xform.transform_dir(m_fire_norm);


	m_allow_fire = true;
	Fmatrix XFi;
	XFi.invert(m_object->XFORM());
	Fvector dep;

#ifdef CHOLDERCUSTOM_CHANGE
	/*
		Take m_i_bind_y_xform as base for both bone x and bone_rotate_y assuming bone_rotate_y always has 0 pitch.
		And reset dep before calculating.
	*/
	{
		// x angle
		if (m_desire_angle_active)
		{
			dep.setHP(m_desire_angle_vector.x, m_desire_angle_vector.y);
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
		if (m_desire_angle_active)
		{
			dep.setHP(m_desire_angle_vector.x, m_desire_angle_vector.y);
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
#else
	XFi.transform_dir(dep, m_destEnemyDir);
	{
		// x angle
		m_i_bind_x_xform.transform_dir(dep);
		dep.normalize();
		m_tgt_x_rot = angle_normalize_signed(m_bind_x_rot - dep.getP());
		clamp(m_tgt_x_rot, -m_lim_x_rot.y, -m_lim_x_rot.x);
	}
	{
		// y angle
		m_i_bind_y_xform.transform_dir(dep);
		dep.normalize();
		m_tgt_y_rot = angle_normalize_signed(m_bind_y_rot - dep.getH());
		clamp(m_tgt_y_rot, -m_lim_y_rot.y, -m_lim_y_rot.x);
	}
#endif

	m_cur_x_rot = angle_inertion_var(m_cur_x_rot, m_tgt_x_rot, m_min_gun_speed, m_max_gun_speed, PI, Device.fTimeDelta);
	m_cur_y_rot = angle_inertion_var(m_cur_y_rot, m_tgt_y_rot, m_min_gun_speed, m_max_gun_speed, PI, Device.fTimeDelta);
	static float dir_eps = deg2rad(5.0f);
	if (!fsimilar(m_cur_x_rot, m_tgt_x_rot, dir_eps) || !fsimilar(m_cur_y_rot, m_tgt_y_rot, dir_eps))
		m_allow_fire = FALSE;

#if (0)
	if(Device.dwFrame%200==0){
		Msg("m_cur_x_rot=[%f]",m_cur_x_rot);
		Msg("m_cur_y_rot=[%f]",m_cur_y_rot);
	}
#endif
}

bool CCarWeapon::AllowFire()
{
	return m_allow_fire;
}

float CCarWeapon::FireDirDiff()
{
	Fvector d1, d2;
	d1.set(m_cur_x_rot, m_cur_y_rot, 0).normalize_safe();
	d2.set(m_tgt_x_rot, m_tgt_y_rot, 0).normalize_safe();
	return rad2deg(acos(d1.dotproduct(d2)));
}

const Fvector& CCarWeapon::get_CurrentFirePoint()
{
	return m_fire_pos;
}

const Fmatrix& CCarWeapon::get_ParticlesXFORM()
{
	return m_fire_bone_xform;
}

void CCarWeapon::FireStart()
{
	inheritedShooting::FireStart();
}

void CCarWeapon::FireEnd()
{
	inheritedShooting::FireEnd();
	StopFlameParticles();
#ifdef CHOLDERCUSTOM_CHANGE
	RemoveShotEffector();
#endif
}

#ifdef CHOLDERCUSTOM_CHANGE
void CCarWeapon::OnShot()
{
	CGameObject *gunner = m_holder->Gunner();
	gunner = (gunner) ? gunner : m_object->cast_game_object();

	if (m_rocket_enable)
	{
		FireRocket(gunner);
	}
	else
	{
		CCartridge l_cartridge = (m_magazine.size() > 0) ? m_magazine.back() : m_DefaultCartridge;
		FireBullet(m_fire_pos, m_fire_dir, fireDispersionBase, l_cartridge, gunner->ID(), m_object->ID(), SendHitAllowed(m_object), GetAmmoElapsed());
	}

	if (!m_unlimited_ammo)
	{
		m_magazine.pop_back();
		--iAmmoElapsed;
		VERIFY((u32)iAmmoElapsed == m_magazine.size());
	}
	UpdateRocketMagazine();
	UpdateAmmoVisibility();

	StartShotParticles();
	if (m_bLightShotEnabled)
		Light_Start();
	StartFlameParticles();
	StartSmokeParticles(m_fire_pos, zero_vel);
	m_sounds.PlaySound("sndShot", m_fire_pos, gunner, false);

	if (m_object->PPhysicsShell())
	{
		/* Drop casing. */
		if (m_drop_bone != BI_NONE)
		{
			Fvector drop_pos;
			m_object->XFORM().transform_tiny(drop_pos, m_object->Visual()->dcast_PKinematics()->LL_GetTransform(m_drop_bone).c);
			Fvector drop_vel;
			m_object->PPhysicsShell()->get_LinearVel(drop_vel);
			OnShellDrop(drop_pos, drop_vel);
		}
		/* Recoil. */
		CPhysicsElement *E = m_object->PPhysicsShell()->get_Element(m_rotate_x_bone);
		if (E)
		{
			Fvector vec = Fvector().set(0.0F, 0.0F, -1.0F);
			Fmatrix xfm = Fmatrix().mul_43(m_object->XFORM(), m_object->Visual()->dcast_PKinematics()->LL_GetTransform(m_rotate_x_bone));
			xfm.transform_dir(vec);
			vec.normalize_safe();
			E->applyForce(vec, m_recoil_force / 0.02F);
		}
	}
}
#else
void CCarWeapon::OnShot()
{
	CHolderCustom* holder = smart_cast<CHolderCustom*>(m_object);
	FireBullet(m_fire_pos, m_fire_dir, fireDispersionBase, *m_Ammo, holder->Engaged() ? 0 : m_object->ID(), m_object->ID(), SendHitAllowed(m_object), ::Random.randI(0, 30));

	StartShotParticles();

	if (m_bLightShotEnabled)
		Light_Start();

	StartFlameParticles();
	StartSmokeParticles(m_fire_pos, zero_vel);
	//	OnShellDrop				(m_fire_pos, zero_vel);

	HUD_SOUND_ITEM::PlaySound(m_sndShot, m_fire_pos, m_object, false);
}
#endif

void CCarWeapon::Action(u16 id, u32 flags)
{
	switch (id)
	{
	case eWpnFire:
		{
#ifdef CHOLDERCUSTOM_CHANGE
			if (flags == 1)
				SwitchState(eCarWeaponStateFire);
			else
				SwitchState(eCarWeaponStateIdle);
#else
			if (flags == 1) FireStart();
			else FireEnd();
#endif
		}
		break;
	case eWpnActivate:
		{
#ifdef CHOLDERCUSTOM_CHANGE
			if (flags == 1 && m_bActive != true)
			{
				m_bActive = true;
				SetBoneCallbacks();
			}
			if (flags != 1 && m_bActive == true)
			{
				m_bActive = false;
				FireEnd();
				ResetBoneCallbacks();
			}
#else
			if (flags == 1) m_bActive = true;
			else
			{
				m_bActive = false;
				FireEnd();
			}
#endif
		}
		break;

	case eWpnAutoFire:
		{
			if (flags == 1) m_bAutoFire = true;
			else m_bAutoFire = false;
		}
		break;
	case eWpnToDefaultDir:
		{
			SetParam(eWpnDesiredDir, Fvector2().set(m_bind_y_rot, m_bind_x_rot));
		}
		break;
#ifdef CHOLDERCUSTOM_CHANGE
	case eWpnReload:
		{
			SwitchState(eCarWeaponStateReload);
		}
		break;
	case eWpnUnload:
		{
			SwitchState(eCarWeaponStateUnload);
		}
		break;
#endif
	}
}

void CCarWeapon::SetParam(int id, Fvector2 val)
{
	switch (id)
	{
	case eWpnDesiredDir:
		m_destEnemyDir.setHP(val.x, val.y);
#ifdef CHOLDERCUSTOM_CHANGE
		m_desire_angle_active = false;
#endif
		break;
#ifdef CHOLDERCUSTOM_CHANGE
	case eWpnDesiredAng:
		m_desire_angle_vector.set(val);
		m_desire_angle_active = true;
#endif
	}
}

void CCarWeapon::SetParam(int id, Fvector val)
{
	switch (id)
	{
	case eWpnDesiredPos:
		m_destEnemyDir.sub(val, m_fire_pos).normalize_safe();
#ifdef CHOLDERCUSTOM_CHANGE
		m_desire_angle_active = false;
#endif
		break;
	}
}

const Fvector& CCarWeapon::ViewCameraPos()
{
	return m_fire_pos;
}

const Fvector& CCarWeapon::ViewCameraDir()
{
	return m_fire_dir;
}

const Fvector& CCarWeapon::ViewCameraNorm()
{
	return m_fire_norm;
}

#ifdef CHOLDERCUSTOM_CHANGE
CActor *CCarWeapon::GunnerActor()
{
	CGameObject *gunner = m_holder->Gunner();
	return (gunner) ? gunner->cast_actor() : nullptr;
}

bool CCarWeapon::IsReloadConsumeInventory()
{
	/* Default unlimited ammo. */
	if (m_reload_consume_inventory_callback && strlen(m_reload_consume_inventory_callback))
	{
		if (strcmp(m_reload_consume_inventory_callback, "true") == 0)
		{
			return true;
		}
		luabind::functor<bool> lua_function;
		if (ai().script_engine().functor(m_reload_consume_inventory_callback, lua_function))
		{
			return (lua_function(m_object->lua_game_object(), m_holder->Gunner())) ? true : false;
		}
	}
	return false;
}

void CCarWeapon::AddShotEffector()
{
	if (GunnerActor())
	{
		CCameraShotEffector *S = smart_cast<CCameraShotEffector *>(Actor()->Cameras().GetCamEffector(eCEShot));
		CameraRecoil cam_recoil;
		cam_recoil.RelaxSpeed = deg2rad(10.0F);
		cam_recoil.MaxAngleVert = 0.0F;
		cam_recoil.MaxAngleHorz = deg2rad(20.0F);
		cam_recoil.DispersionFrac = 0.7f;
		cam_recoil.StepAngleHorz = ::Random.randF(-1.0f, 1.0f) * 0.01f;

		if (S == NULL)
			S = (CCameraShotEffector *)Actor()->Cameras().AddCamEffector(xr_new<CCameraShotEffector>(cam_recoil));
		R_ASSERT(S);
		S->Initialize(cam_recoil);
		S->Shot2(0.01f);
	}
}

void CCarWeapon::RemoveShotEffector()
{
	if (GunnerActor())
	{
		Actor()->Cameras().RemoveCamEffector(eCEShot);
	}
}

Fvector CCarWeapon::GetBasePos()
{
	Fvector vec;
	m_object->XFORM().transform_tiny(vec, m_object->Visual()->dcast_PKinematics()->LL_GetTransform(m_rotate_y_bone).c);
	return vec;
}

void CCarWeapon::OnAttachCrew()
{
	m_bActive = true;
	FireEnd();
	SwitchState(eCarWeaponStateIdle);
}

void CCarWeapon::OnDetachCrew()
{
	m_bActive = false;
	FireEnd();
	SwitchState(eCarWeaponStateIdle);
}

void CCarWeapon::SwitchState(u32 state)
{
	switch (state)
	{
	case eCarWeaponStateIdle:
		switch2_Idle();
		break;
	case eCarWeaponStateFire:
		switch2_Fire();
		break;
	case eCarWeaponStateReload:
		switch2_Reload();
		break;
	case eCarWeaponStateUnload:
		switch2_Unload();
		break;
	}
}

void CCarWeapon::switch2_Idle()
{
	m_state_index = eCarWeaponStateIdle;
	m_state_delay = 0;
	m_iShotNum = 0;
	FireEnd();
}

void CCarWeapon::switch2_Fire()
{
	m_state_index = eCarWeaponStateFire;
	m_state_delay = 0;
	if (IsWorking() == TRUE)
	{
		return;
	}
	m_bFireSingleShot = true;
	m_iShotNum = 0;
	FireStart();
}

void CCarWeapon::switch2_Reload()
{
	if (GetAmmoElapsed() == GetAmmoMagSize())
		return;
	if (IsReloadConsumeInventory() && (GetAmmoCount(m_ammoType) > 0))
		return;
	CAI_Stalker *who = smart_cast<CAI_Stalker *>(m_holder->Gunner());
	if (who)
	{
		/* Play sound or something. */
	}
	FireEnd();
	m_state_index = eCarWeaponStateReload;
	m_state_delay = 3;
}

void CCarWeapon::switch2_Unload()
{
	if (GetAmmoElapsed() == 0)
	{
		return;
	}
	FireEnd();
	m_state_index = eCarWeaponStateUnload;
	m_state_delay = 3;
}

void CCarWeapon::UpdateReload()
{
	m_state_delay = m_state_delay - Device.fTimeDelta;
	if (m_state_delay < 0)
	{
		m_state_index = eCarWeaponStateIdle;
		m_state_delay = 0;
		ReloadMagazine();
	}
}

void CCarWeapon::UpdateUnload()
{
	m_state_delay = m_state_delay - Device.fTimeDelta;
	if (m_state_delay < 0)
	{
		m_state_index = eCarWeaponStateIdle;
		m_state_delay = 0;
		UnloadMagazine(true);
	}
}

void CCarWeapon::FireRocket(CGameObject *gunner)
{
	if (getRocketCount() > 0)
	{
		CExplosiveRocket *rocket = smart_cast<CExplosiveRocket *>(getCurrentRocket());
		VERIFY(rocket);
		rocket->SetInitiator(gunner->ID());

		Fmatrix xfm = Fmatrix().mul_43(m_object->XFORM(), m_object->Visual()->dcast_PKinematics()->LL_GetTransform(m_fire_bone));
		Fvector dir = Fvector().set(0.0F, 0.0F, 1.0F);
		xfm.transform_dir(dir);
		dir.normalize_safe();
		Fvector vel = Fvector().mul(dir, CRocketLauncher::m_fLaunchSpeed);

		Fmatrix xform;
		xform.identity();
		xform.k.set(dir);
		Fvector::generate_orthonormal_basis(xform.k, xform.j, xform.i);
		xform.translate_over(xfm.c);

		LaunchRocket(xform, vel, Fvector().set(0.0F, 0.0F, 0.0F));

		NET_Packet P;
		CGameObject::u_EventGen(P, GE_LAUNCH_ROCKET, m_object->ID());
		P.w_u16(u16(getCurrentRocket()->ID()));
		CGameObject::u_EventSend(P);
		dropCurrentRocket();
	}
}

void CCarWeapon::SetAmmoElapsed(int ammo_count)
{
	iAmmoElapsed = ammo_count;
	clamp(iAmmoElapsed, 0, iMagazineSize);
	u32 uAmmo = u32(iAmmoElapsed);
	if (uAmmo != m_magazine.size())
	{
		if (uAmmo > m_magazine.size())
		{
			CCartridge l_cartridge;
			l_cartridge.Load(m_ammoTypes[m_ammoType].c_str(), m_ammoType, false);
			while (uAmmo > m_magazine.size())
				m_magazine.push_back(l_cartridge);
		}
		else
		{
			while (uAmmo < m_magazine.size())
				m_magazine.pop_back();
		}
	}
	UpdateRocketMagazine();
	UpdateAmmoVisibility();
}

void CCarWeapon::ReloadMagazine()
{
	UnloadMagazine(true);
	AddCartridge(1);
}

void CCarWeapon::UnloadMagazine(bool spawn_ammo)
{
	xr_map<LPCSTR, u16> l_ammo;
	while (!m_magazine.empty())
	{
		CCartridge &l_cartridge = m_magazine.back();
		xr_map<LPCSTR, u16>::iterator l_it;
		for (l_it = l_ammo.begin(); l_ammo.end() != l_it; ++l_it)
		{
			if (!xr_strcmp(*l_cartridge.m_ammoSect, l_it->first))
			{
				++(l_it->second);
				break;
			}
		}

		if (l_it == l_ammo.end())
		{
			l_ammo[*l_cartridge.m_ammoSect] = 1;
		}
		m_magazine.pop_back();
		--iAmmoElapsed;
	}

	UnloadRocket();
	UpdateRocketMagazine();
	UpdateAmmoVisibility();
}

u16 CCarWeapon::AddCartridge(u16 cnt)
{
	if (iAmmoElapsed >= iMagazineSize)
		return 0;
	m_pCurrentAmmo = smart_cast<CWeaponAmmo *>(m_holder->GetInventory()->GetAny(m_ammoTypes[m_ammoType].c_str()));
	if (m_DefaultCartridge.m_LocalAmmoType != m_ammoType)
	{
		m_DefaultCartridge.Load(m_ammoTypes[m_ammoType].c_str(), m_ammoType, 1.0F);
	}
	CCartridge l_cartridge = m_DefaultCartridge;
	while (cnt)
	{
		if (IsReloadConsumeInventory() && !m_pCurrentAmmo->Get(l_cartridge))
		{
			break;
		}
		--cnt;
		++iAmmoElapsed;
		l_cartridge.m_LocalAmmoType = m_ammoType;
		m_magazine.push_back(l_cartridge);
	}
	UpdateRocketMagazine();
	UpdateAmmoVisibility();
	return cnt;
}

int CCarWeapon::GetAmmoCount(u8 ammo_type) const
{
	VERIFY(m_holder->GetInventory());
	R_ASSERT(ammo_type < m_ammoTypes.size());
	return GetAmmoCount_forType(m_ammoTypes[ammo_type]);
}

int CCarWeapon::GetAmmoCount_forType(shared_str const &ammo_type) const
{
	CInventory *inv = m_holder->GetInventory();
	int res = 0;
	TIItemContainer::iterator itb = inv->m_all.begin();
	TIItemContainer::iterator ite = inv->m_all.end();
	for (; itb != ite; ++itb)
	{
		CWeaponAmmo *pAmmo = smart_cast<CWeaponAmmo *>(*itb);
		if (pAmmo && (pAmmo->cNameSect() == ammo_type))
		{
			res += pAmmo->m_boxCurr;
		}
	}
	return res;
}

void CCarWeapon::UnloadRocket()
{
	if (IsRocketLauncher())
	{
		while (getRocketCount() > 0)
		{
			NET_Packet P;
			CGameObject::u_EventGen(P, GE_OWNERSHIP_REJECT, m_object->ID());
			P.w_u16(u16(getCurrentRocket()->ID()));
			CGameObject::u_EventSend(P);
			dropCurrentRocket();
		}
	}
}

void CCarWeapon::UpdateRocketMagazine()
{
	if (IsRocketLauncher())
	{
		while (getRocketCount() != iAmmoElapsed)
		{
			if (getRocketCount() < iAmmoElapsed)
			{
				shared_str fake_grenade_name = pSettings->r_string(m_ammoTypes[m_ammoType].c_str(), "fake_grenade_name");
				CRocketLauncher::SpawnRocket(fake_grenade_name.c_str(), m_object);
			}
			else
			{
				NET_Packet P;
				CGameObject::u_EventGen(P, GE_OWNERSHIP_REJECT, m_object->ID());
				P.w_u16(u16(getCurrentRocket()->ID()));
				CGameObject::u_EventSend(P);
				dropCurrentRocket();
			}
		}
	}
}

void CCarWeapon::UpdateAmmoVisibility()
{
	if (m_ammo_bone != BI_NONE)
	{
		BOOL visible = (GetAmmoElapsed() > 0) ? TRUE : FALSE;
		m_object->Visual()->dcast_PKinematics()->LL_SetBoneVisible(m_ammo_bone, visible, TRUE);
	}
}
#endif
