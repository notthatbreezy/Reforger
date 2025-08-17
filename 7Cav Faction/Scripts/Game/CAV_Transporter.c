// ============================================================================
//  CAV_Transporter.c  (Arma Reforger / Enfusion Script)
//  Teleport network with "one action per destination"
// ============================================================================

// ---------------------------------------------------------------------------
// CAV_TransportNodeComponent
// ---------------------------------------------------------------------------
[BaseContainerProps(), ComponentEditorProps(category: "CAV/Transport", description: "Teleport/transport node")]
class CAV_TransportNodeComponentClass : ScriptComponentClass {}

class CAV_TransportNodeComponent : ScriptComponent
{
	[Attribute("Transport Node", UIWidgets.EditBox, "Friendly name shown in actions; GM can edit after placement")]
	protected string m_NodeName;

	[Attribute("3.0", UIWidgets.Slider, "Use radius (meters) to show actions", "0.5 20 0.5")]
	protected float m_UseRadius;

	[Attribute("0.7", UIWidgets.Slider, "Vertical offset added at destination (meters)", "-2 2 0.1")]
	protected float m_LandingYOffset;

	[Attribute("false", UIWidgets.CheckBox, "Debug prints for this node")]
	protected bool m_Debug;

	protected RplComponent m_Rpl;
	protected ref array<CAV_TransportToAction> m_SlotActions = {};
	protected vector m_TempMat[4];

	IEntity GetOwnerEntity() { return GetOwner(); }
	string GetNodeName() { return m_NodeName; }
	float  GetUseRadius() { return m_UseRadius; }
	float  GetLandingYOffset() { return m_LandingYOffset; }
	bool   IsDebug() { return m_Debug; }

	vector GetWorldPos()
	{
		IEntity owner = GetOwner();
		if (!owner) return "0 0 0";
		owner.GetWorldTransform(m_TempMat);
		return m_TempMat[3];
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_Rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (!m_Rpl && m_Debug)
			Print("[CAV_TP] Warning: Node has no RplComponent (required for networking)");

		CAV_TransportNetworkComponent net = CAV_TransportNetworkComponent.Get();
		if (net && Replication.IsServer())
		{
			net.RegisterNode(this);
			if (m_Debug) PrintFormat("[CAV_TP] Registered node '%1' (%2)", m_NodeName, owner);
		}

		CollectSlotActions();
		RefreshSlots();
	}

	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
		{
			CAV_TransportNetworkComponent net = CAV_TransportNetworkComponent.Get();
			if (net) net.UnregisterNode(this);
		}
		super.OnDelete(owner);
	}

	void TopologyChanged()
	{
		RefreshSlots();
		if (m_Debug) PrintFormat("[CAV_TP] TopologyChanged -> refreshed slots for '%1'", m_NodeName);
	}

	protected void CollectSlotActions()
	{
		m_SlotActions.Clear();

		ActionsManagerComponent am = ActionsManagerComponent.Cast(GetOwner().FindComponent(ActionsManagerComponent));
		if (!am)
		{
			if (m_Debug) Print("[CAV_TP] No ActionsManagerComponent on node prefab");
			return;
		}

		array<BaseUserAction> actions = {};
		am.GetActionsList(actions);

		foreach (BaseUserAction baseAct : actions)
		{
			CAV_TransportToAction tp = CAV_TransportToAction.Cast(baseAct);
			if (!tp) continue;

			tp.BindOwnerNode(this);
			m_SlotActions.Insert(tp);
		}

		ManualSortSlotActionsByIndex();

		if (m_Debug) PrintFormat("[CAV_TP] Found %1 slot actions", m_SlotActions.Count());
	}

	protected void RefreshSlots()
	{
		CAV_TransportNetworkComponent net = CAV_TransportNetworkComponent.Get();
		if (!net) return;

		array<CAV_TransportNodeComponent> all = {};
		net.GetAllNodes(all);

		ref array<ref CAV_TpDest> dests = new array<ref CAV_TpDest>();
		foreach (CAV_TransportNodeComponent other : all)
		{
			if (other == this) continue;
			dests.Insert(new CAV_TpDest(other));
		}

		int used = Math.Min(dests.Count(), m_SlotActions.Count());
		for (int i = 0; i < used; i++)
		{
			m_SlotActions[i].ConfigureForDestination(dests[i]);
			if (m_Debug) PrintFormat("[CAV_TP] Slot %1 -> %2", m_SlotActions[i].GetSlotIndex(), dests[i].m_Name);
		}
		for (int j = used; j < m_SlotActions.Count(); j++)
		{
			m_SlotActions[j].ClearDestination();
		}
	}

	protected void ManualSortSlotActionsByIndex()
	{
		int n = m_SlotActions.Count();
		for (int i = 0; i < n - 1; i++)
		{
			int minIdx = i;
			for (int j = i + 1; j < n; j++)
			{
				if (m_SlotActions[j].GetSlotIndex() < m_SlotActions[minIdx].GetSlotIndex())
					minIdx = j;
			}
			if (minIdx != i)
			{
				CAV_TransportToAction tmp = m_SlotActions[i];
				m_SlotActions[i] = m_SlotActions[minIdx];
				m_SlotActions[minIdx] = tmp;
			}
		}
	}
};

class CAV_TpDest
{
	int    m_RplId;
	string m_Name;
	vector m_WorldPos;

	void CAV_TpDest(CAV_TransportNodeComponent node)
	{
		IEntity ent = node.GetOwnerEntity();
		RplComponent rpl = RplComponent.Cast(ent.FindComponent(RplComponent));
		if (rpl)
			m_RplId = rpl.Id();
		else
			m_RplId = -1;

		m_Name = node.GetNodeName();
		m_WorldPos = node.GetWorldPos();
	}
}


// ---------------------------------------------------------------------------
// CAV_TransportNetworkComponent (server authoritative)
// ---------------------------------------------------------------------------
[BaseContainerProps(), ComponentEditorProps(category: "CAV/Transport", description: "Teleport/transport node")]
class CAV_TransportNetworkComponentClass : ScriptComponentClass {}

class CAV_TransportNetworkComponent : SCR_BaseGameModeComponentClass
{
	[Attribute("false", UIWidgets.CheckBox, "Debug prints for network (server)")]
	protected bool m_Debug;

	protected ref map<int, CAV_TransportNodeComponent> m_Nodes = new map<int, CAV_TransportNodeComponent>();

	static CAV_TransportNetworkComponent Get()
	{
		BaseGameMode gm = GetGame().GetGameMode();
		if (gm)
			return CAV_TransportNetworkComponent.Cast(gm.FindComponent(CAV_TransportNetworkComponent));
		return null;
	}

	void RegisterNode(CAV_TransportNodeComponent node)
	{
		if (!Replication.IsServer()) return;

		IEntity ent = node.GetOwnerEntity();
		RplComponent rpl = RplComponent.Cast(ent.FindComponent(RplComponent));
		if (!rpl) return;

		m_Nodes.Set(rpl.Id(), node);
		if (m_Debug) PrintFormat("[CAV_TP_NET] Registered %1 (id=%2)", node.GetNodeName(), rpl.Id());
		BroadcastTopologyChanged();
	}

	void UnregisterNode(CAV_TransportNodeComponent node)
	{
		if (!Replication.IsServer()) return;

		IEntity ent = node.GetOwnerEntity();
		RplComponent rpl = RplComponent.Cast(ent.FindComponent(RplComponent));
		if (!rpl) return;

		m_Nodes.Remove(rpl.Id());
		if (m_Debug) PrintFormat("[CAV_TP_NET] Unregistered %1 (id=%2)", node.GetNodeName(), rpl.Id());
		BroadcastTopologyChanged();
	}

	void BroadcastTopologyChanged()
	{
		foreach (int id, CAV_TransportNodeComponent n : m_Nodes)
			n.TopologyChanged();
	}

	void GetAllNodes(out array<CAV_TransportNodeComponent> outNodes)
	{
		if (!outNodes) outNodes = new array<CAV_TransportNodeComponent>();
		outNodes.Clear();
		foreach (int id, CAV_TransportNodeComponent n : m_Nodes)
			outNodes.Insert(n);
	}

	// NOTE: RPC now takes the requester PLAYER ID (not an entity RplId)
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_TeleportTo(int destNodeRplId, int requesterPlayerId)
	{
		if (!Replication.IsServer()) return;

		CAV_TransportNodeComponent dest = m_Nodes.Get(destNodeRplId);
		if (!dest)
		{
			if (m_Debug) Print("[CAV_TP_NET] Reject: unknown destination id");
			return;
		}

		if (requesterPlayerId < 0)
		{
			if (m_Debug) Print("[CAV_TP_NET] Reject: invalid requester playerId");
			return;
		}

		vector pos = dest.GetWorldPos();
		pos[1] = pos[1] + dest.GetLandingYOffset();

		bool ok = SCR_Global.TeleportPlayer(requesterPlayerId, pos, SCR_EPlayerTeleportedReason.DEFAULT);
		if (m_Debug) PrintFormat("[CAV_TP_NET] Teleport playerId=%1 -> '%2' (ok=%3)", requesterPlayerId, dest.GetNodeName(), ok);
	}
}


// ---------------------------------------------------------------------------
// CAV_TransportToAction  (one copy per slot on the node prefab)
// ---------------------------------------------------------------------------
class CAV_TransportToAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.Slider, "Slot Index (0..N-1). Make one action per slot.", "0 63 1")]
	protected int m_SlotIndex;

	[Attribute("false", UIWidgets.CheckBox, "Debug prints for this action")]
	protected bool m_Debug;

	protected CAV_TransportNodeComponent m_OwnerNode;

	protected int    m_TargetRpl = -1;
	protected string m_TargetName = "";
	protected bool   m_HasDest = false;

	int GetSlotIndex() { return m_SlotIndex; }

	void BindOwnerNode(CAV_TransportNodeComponent node)
	{
		m_OwnerNode = node;
	}

	void ConfigureForDestination(CAV_TpDest dest)
	{
		m_TargetRpl = dest.m_RplId;
		m_TargetName = dest.m_Name;
		m_HasDest = (m_TargetRpl >= 0);
		if (m_Debug || (m_OwnerNode && m_OwnerNode.IsDebug()))
			PrintFormat("[CAV_TP_ACT] Slot #%1 configured -> %2", m_SlotIndex, m_TargetName);
	}

	void ClearDestination()
	{
		m_TargetRpl = -1;
		m_TargetName = "";
		m_HasDest = false;
	}

	override bool GetActionNameScript(out string outName)
	{
		if (!m_HasDest) return false;
		outName = string.Format("Transport to %1", m_TargetName);
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		if (!m_HasDest || !m_OwnerNode) return false;

		IEntity owner = GetOwner();
		if (!owner || !user) return false;

		float r = m_OwnerNode.GetUseRadius();
		return vector.Distance(user.GetOrigin(), owner.GetOrigin()) <= r;
	}

	override bool CanBePerformedScript(IEntity user)
	{
		return CanBeShownScript(user);
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!m_HasDest || !m_OwnerNode) return;

		CAV_TransportNetworkComponent net = CAV_TransportNetworkComponent.Get();
		if (!net) return;

		// Get the calling player's ID client-side and send it
		int requesterPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (requesterPlayerId < 0) return;

		if (m_Debug || m_OwnerNode.IsDebug())
			PrintFormat("[CAV_TP_ACT] Requesting teleport -> %1 (playerId=%2)", m_TargetName, requesterPlayerId);

		// Call the annotated RPC directly on the network component
		net.RpcAsk_TeleportTo(m_TargetRpl, requesterPlayerId);
	}

	override bool HasLocalEffectOnlyScript() { return false; }
}
