#pragma once
#include "ShootingObject.h"
#include "HudSound.h"
#include "holder_custom.h"

#ifdef CHOLDERCUSTOM_CHANGE
#include "Level.h"
#include "../xrphysics/IPHWorld.h"
#include "ai/stalker/ai_stalker.h"
#include "Actor.h"

#include "Inventory.h"
#include "WeaponAmmo.h"
#include "RocketLauncher.h"
#include "CustomRocket.h"
#include "ExplosiveRocket.h"

#include "ActorEffector.h"
#include "EffectorShot.h"
class CInventory;
#endif

class CPhysicsShellHolder;

class CCarWeapon : public CShootingObject
#ifdef CHOLDERCUSTOM_CHANGE
, public CRocketLauncher
#endif
{
protected:
	typedef CShootingObject inheritedShooting;

	void SetBoneCallbacks();
	void ResetBoneCallbacks();
	virtual void FireStart();
	virtual void FireEnd();
	virtual void UpdateFire();
	virtual void OnShot();
	void UpdateBarrelDir();
	virtual const Fvector& get_CurrentFirePoint();
	virtual const Fmatrix& get_ParticlesXFORM();

	CPhysicsShellHolder* m_object;
	bool m_bActive;
	bool m_bAutoFire;
	float m_weapon_h;
	virtual bool IsHudModeNow() { return false; };

public:
	enum
	{
		eWpnDesiredDir =1,
		eWpnDesiredPos,
		eWpnActivate,
		eWpnFire,
		eWpnAutoFire,
		eWpnToDefaultDir,
#ifdef CHOLDERCUSTOM_CHANGE
		eWpnDesiredAng,
		eWpnReload,
		eWpnUnload,
#endif
	};

	CCarWeapon(CPhysicsShellHolder* obj);
	virtual ~CCarWeapon();
	static void _BCL BoneCallbackX(CBoneInstance* B);
	static void _BCL BoneCallbackY(CBoneInstance* B);
	void Load(LPCSTR section);
	void UpdateCL();
	void Action(u16 id, u32 flags);
	void SetParam(int id, Fvector2 val);
	void SetParam(int id, Fvector val);
	bool AllowFire();
	float FireDirDiff();
	IC bool IsActive() { return m_bActive; }
	float _height() const { return m_weapon_h; };
	const Fvector& ViewCameraPos();
	const Fvector& ViewCameraDir();
	const Fvector& ViewCameraNorm();

	void Render_internal();
private:
	u16 m_rotate_x_bone, m_rotate_y_bone, m_fire_bone, m_camera_bone;
	float m_tgt_x_rot, m_tgt_y_rot, m_cur_x_rot, m_cur_y_rot, m_bind_x_rot, m_bind_y_rot;
	Fvector m_bind_x, m_bind_y;
	Fvector m_fire_dir, m_fire_pos, m_fire_norm;

	Fmatrix m_i_bind_x_xform, m_i_bind_y_xform, m_fire_bone_xform;
	Fvector2 m_lim_x_rot, m_lim_y_rot; //in bone space
	float m_min_gun_speed, m_max_gun_speed;
	CCartridge* m_Ammo;
	float m_barrel_speed;
	Fvector m_destEnemyDir;
	bool m_allow_fire;
	HUD_SOUND_ITEM m_sndShot;

#ifdef CHOLDERCUSTOM_CHANGE
/*----------------------------------------------------------------------------------------------------
	Misc
----------------------------------------------------------------------------------------------------*/
private:
	CHolderCustom *m_holder;

	u16 m_drop_bone;
	u16 m_ammo_bone;
	float m_recoil_force;

	Fvector2 m_desire_angle_vector;
	bool m_desire_angle_active;

	HUD_SOUND_COLLECTION_LAYERED m_sounds;
	void AddShotEffector();
	void RemoveShotEffector();
	CActor *GunnerActor();

public:
	void OnAttachCrew();
	void OnDetachCrew();
	Fvector GetBasePos();
	Fvector GetFirePos() { return m_fire_pos; }
	Fvector GetFireDir() { return m_fire_dir; }

/*----------------------------------------------------------------------------------------------------
	Ammo
----------------------------------------------------------------------------------------------------*/
private:
	bool m_unlimited_ammo;
	LPCSTR m_reload_consume_inventory_callback;
	bool IsReloadConsumeInventory();

	bool m_rocket_enable;
	void FireRocket(CGameObject *gunner);

	u32 m_state_index;
	float m_state_delay;

public:
	enum ECarWeaponStates
	{
		eCarWeaponStateIdle = 0,
		eCarWeaponStateFire,
		eCarWeaponStateReload,
		eCarWeaponStateUnload,
	};
protected:
	IC u32 GetState() const { return m_state_index; }
	virtual void SwitchState(u32 new_tate);
	virtual void switch2_Idle();
	virtual void switch2_Fire();
	virtual void switch2_Reload();
	virtual void switch2_Unload();

	virtual void UpdateReload();
	virtual void UpdateUnload();

public:
	bool IsRocketLauncher() { return m_rocket_enable; };

	CWeaponAmmo *m_pCurrentAmmo;
	xr_vector<shared_str> m_ammoTypes;
	u8 m_ammoType;

	xr_vector<CCartridge> m_magazine;
	CCartridge m_DefaultCartridge;
	int iAmmoElapsed;
	int iMagazineSize;

	virtual void ReloadMagazine();
	virtual void UnloadMagazine(bool spawn_ammo = true);
	
	virtual void UnloadRocket();
	virtual void UpdateRocketMagazine();
	virtual void UpdateAmmoVisibility();

protected:
	int m_iShotNum;
	bool m_bFireSingleShot;
	bool m_needReload;
	bool m_needUnload;
	//virtual void OnMagazineEmpty();

	int GetAmmoCount(u8 ammo_type) const;
	int GetAmmoCount_forType(shared_str const& ammo_type) const;
	virtual u16 AddCartridge(u16 cnt);

public:
	void SetAmmoElapsed(int ammo_count);

	IC int GetAmmoElapsed() const
	{
		return iAmmoElapsed;
	}

	IC u8 GetAmmoType() const
	{
		return m_ammoType;
	}

	IC int GetAmmoMagSize() const
	{
		return iMagazineSize;
	}
#endif
};