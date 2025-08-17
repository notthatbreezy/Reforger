
[BaseContainerProps()]
class CAV_PvE_GameModeClass : SCR_BaseGameModeClass
{
}

[BaseContainerProps()]
class CAV_PvE_GameMode : SCR_BaseGameMode
{
	override void OnGameStart()
	{
		super.OnGameStart();
		Print("[CAV_PVE] Game start on " + GetGame().GetWorldEntity().GetName(), LogLevel.NORMAL);

		SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!fm)
			Debug.Error("[CAV_PVE] No SCR_FactionManager present — add one to the GameMode prefab.");
	}

	// Called on all machines after identity is registered; do server-only work here
	override void OnPlayerRegistered(int playerId)
	{
		super.OnPlayerRegistered(playerId);
		if (!Replication.IsServer()) return; // only the server should assign factions. :contentReference[oaicite:1]{index=1}

		SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!fm) return;

		// Prefer US; otherwise fall back to first available
		Faction target = fm.GetFactionByKey("US");
		if (!target)
		{
			SCR_SortedArray<SCR_Faction> list();
			if (fm.GetSortedFactionsList(list) > 0) target = list[0];
		}
		if (!target) return;

		// Get the player's controller, then either force-set or request a faction
		PlayerManager pm = GetGame().GetPlayerManager();
		PlayerController pc = pm.GetPlayerController(playerId);  // controller of this player :contentReference[oaicite:2]{index=2}
		if (!pc) return;

		// Server-authoritative assignment (one-liner):
		// This updates the affiliation component on the PlayerController.
		SCR_FactionAffiliationComponent.SetFaction(pc, target);  // static helper on the base affiliation component :contentReference[oaicite:3]{index=3}

		// If you ever need to do it via the component API instead:
		// SCR_PlayerFactionAffiliationComponent aff = SCR_PlayerFactionAffiliationComponent.Cast(
		//     pc.FindComponent(SCR_PlayerFactionAffiliationComponent));
		// if (aff) aff.RequestFaction(target); // owner->server RPC path; best called on the owning client. :contentReference[oaicite:4]{index=4}
	}

	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);
		Print(string.Format("[CAV_PVE] Player %1 spawned %2", playerId, controlledEntity), LogLevel.NORMAL);
	}
}
