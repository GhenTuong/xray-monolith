#include "pch_script.h"
#include "alife_space.h"
#include "Car.h"
#include "CarWeapon.h"
#include "script_game_object.h"

using namespace luabind;

#pragma optimize("s",on)
void CCar::script_register(lua_State* L)
{
	module(L)
	[
		class_<CCar, bases<CGameObject, CHolderCustom>>("CCar")
		.enum_("wpn_action")
		[
			value("eWpnDesiredDir", int(CCarWeapon::eWpnDesiredDir)),
			value("eWpnDesiredPos", int(CCarWeapon::eWpnDesiredPos)),
#ifdef CHOLDERCUSTOM_CHANGE
			value("eWpnDesiredAng", int(CCarWeapon::eWpnDesiredAng)),
#endif
			value("eWpnActivate", int(CCarWeapon::eWpnActivate)),
			value("eWpnFire", int(CCarWeapon::eWpnFire)),
			value("eWpnAutoFire", int(CCarWeapon::eWpnAutoFire)),
			value("eWpnToDefaultDir", int(CCarWeapon::eWpnToDefaultDir))
		]
		.def("Action", &CCar::Action)
		//		.def("SetParam",		(void (CCar::*)(int,Fvector2)) &CCar::SetParam)
		.def("SetParam", (void (CCar::*)(int, Fvector))&CCar::SetParam)
		.def("CanHit", &CCar::WpnCanHit)
		.def("FireDirDiff", &CCar::FireDirDiff)
		.def("IsObjectVisible", &CCar::isObjectVisible)
		.def("HasWeapon", &CCar::HasWeapon)
		.def("CurrentVel", &CCar::CurrentVel)
		.def("GetfHealth", &CCar::GetfHealth)
		.def("SetfHealth", &CCar::SetfHealth)
		.def("SetExplodeTime", &CCar::SetExplodeTime)
		.def("ExplodeTime", &CCar::ExplodeTime)
		.def("CarExplode", &CCar::CarExplode)
		/************************************************** added by Ray Twitty (aka Shadows) START **************************************************/
		.def("GetfFuel", &CCar::GetfFuel)
		.def("SetfFuel", &CCar::SetfFuel)
		.def("GetfFuelTank", &CCar::GetfFuelTank)
		.def("SetfFuelTank", &CCar::SetfFuelTank)
		.def("GetfFuelConsumption", &CCar::GetfFuelConsumption)
		.def("SetfFuelConsumption", &CCar::SetfFuelConsumption)
		.def("ChangefFuel", &CCar::ChangefFuel)
		.def("ChangefHealth", &CCar::ChangefHealth)
		.def("PlayDamageParticles", &CCar::PlayDamageParticles)
		.def("StopDamageParticles", &CCar::StopDamageParticles)
		.def("StartEngine", &CCar::StartEngine)
		.def("StopEngine", &CCar::StopEngine)
		.def("IsActiveEngine", &CCar::isActiveEngine)
		.def("HandBreak", &CCar::HandBreak)
		.def("ReleaseHandBreak", &CCar::ReleaseHandBreak)
		.def("GetRPM", &CCar::GetRPM)
		.def("SetRPM", &CCar::SetRPM)
		/*************************************************** added by Ray Twitty (aka Shadows) END ***************************************************/
		
#ifdef CCAR_CHANGE
		.enum_("car_engine")
		[
			value("eCarEngineStart", int(CCar::eCarEngineStart)),
			value("eCarEngineStartFail", int(CCar::eCarEngineStartFail)),
			value("eCarEngineDontStart", int(CCar::eCarEngineDontStart))
		]

		.def("IsBrp", &CCar::IsBrp)
		.def("IsFwp", &CCar::IsFwp)
		.def("IsBkp", &CCar::IsBkp)
		.def("IsRsp", &CCar::IsRsp)
		.def("IsLsp", &CCar::IsLsp)

		.def("PressBreaks", &CCar::PressBreaks)
		.def("PressForward", &CCar::PressForward)
		.def("PressBack", &CCar::PressBack)
		.def("PressLeft", &CCar::PressLeft)
		.def("PressRight", &CCar::PressRight)

		.def("ReleaseBreaks", &CCar::ReleaseBreaks)
		.def("ReleaseForward", &CCar::ReleaseForward)
		.def("ReleaseBack", &CCar::ReleaseBack)
		.def("ReleaseLeft", &CCar::ReleaseLeft)
		.def("ReleaseRight", &CCar::ReleaseRight)

		.def("SetfMaxPower", &CCar::SetfMaxPower)
		.def("GetfMaxPower", &CCar::GetfMaxPower)
		.def("GetfMaxPowerDef", &CCar::GetfMaxPowerDef)
		.def("GetfFuelTankDef", &CCar::GetfFuelTankDef)
		.def("GetfFuelTankDef", &CCar::GetfFuelTankDef)
		.def("GetfFuelConsumptionDef", &CCar::GetfFuelConsumptionDef)

		.def("StartEngineForce", &CCar::StartEngineForce)
		.def("WeaponGetBasePos", &CCar::WeaponGetBasePos)
		.def("WeaponGetFirePos", &CCar::WeaponGetFirePos)
		.def("WeaponGetFireDir", &CCar::WeaponGetFireDir)

/*----------------------------------------------------------------------------------------------------
	Inventory
----------------------------------------------------------------------------------------------------*/
		.def("HasInventory", &CCar::HasInventory)
		.def("UseInventory", &CCar::UseInventory)
		.def("SetMaxCarryWeight", &CCar::SetMaxCarryWeight)
		.def("GetMaxCarryWeight", &CCar::GetMaxCarryWeight)
		.def("GetMaxCarryWeightDef", &CCar::GetMaxCarryWeightDef)
/*----------------------------------------------------------------------------------------------------
	Crew Manager
----------------------------------------------------------------------------------------------------*/
		.enum_("car_crew")
		[
			value("eCarCrewNone", int(CCar::eCarCrewNone)),
			value("eCarCrewDriver", int(CCar::eCarCrewDriver)),
			value("eCarCrewGunner", int(CCar::eCarCrewGunner)),
			value("eCarCrewMember", int(CCar::eCarCrewMember))
		]
		.def("GetSeatByCrew", &CCar::GetSeatByCrew)
		.def("GetCrewBySeat", &CCar::GetCrewBySeat)
		.def("GetCrewByType", &CCar::GetCrewByType)
		.def("GetCrewType", &CCar::GetCrewType)
		.def("CrewChangeSeat", &CCar::CrewChangeSeat)
#endif
		
		.def(constructor<>())
	];
}