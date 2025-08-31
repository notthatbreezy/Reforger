/*!
  CAV_LayerTaskDestroyAll
  -----------------------
  A LayerTask that completes only when *all* SlotDestroy children under THIS layer's
  entity subtree report SCR_TaskState.FINISHED.

  Add to your LayerTask entity (replace the stock component). Leave Task Prefab = TaskDestroy.et.
*/

[ComponentEditorProps(category: "GameScripted/Scenario/Tasks", description: "Complete when ALL SlotDestroy children finish")]
class CAV_LayerTaskDestroyAllClass : SCR_ScenarioFrameworkLayerTaskClass {} // meta-class required

class CAV_LayerTaskDestroyAll : SCR_ScenarioFrameworkLayerTask
{
	// Debug & tuning
	[Attribute("0", UIWidgets.CheckBox, "Verbose debug logging to console")]
	protected bool m_bDebug;

	[Attribute("500", UIWidgets.EditBox, "Poll interval (ms)")]
	protected int m_iPollMs;

	protected ref array<SCR_ScenarioFrameworkSlotTask> m_DestroySlots = {};
	protected bool m_bWatching = false;
	protected int m_iLastFinished = -1;

	//! Called after children are spawned AND InitTask was queued by the stock layer.
	//! We call super (keeps plugins/actions) then bind our watcher.
	override void AfterAllChildrenSpawned(SCR_ScenarioFrameworkLayerBase layer)
	{
		super.AfterAllChildrenSpawned(layer);
		BindAndStartMultiDestroy();
	}

	//! Discover destroy slots in this layer's subtree and start periodic check.
	protected void BindAndStartMultiDestroy()
	{
		m_DestroySlots.Clear();

		IEntity root = GetOwner();
		if (root)
			GatherDestroySlotsInSubtree(root, m_DestroySlots);

		if (m_bDebug)
		{
			Print(string.Format("[CAV_DestroyAll] Found %1 destroy-slot(s) under this layer", m_DestroySlots.Count()), LogLevel.NORMAL);
			foreach (SCR_ScenarioFrameworkSlotTask s : m_DestroySlots)
			{
				IEntity e = s.GetOwner();
				string nm;
				if (e) nm = e.GetName();
				else nm = "<null-owner>";
				Print("[CAV_DestroyAll]   slot=" + s.ToString() + " owner=" + nm + " state=" + s.GetTaskState(), LogLevel.NORMAL);
			}
		}

		if (m_DestroySlots.IsEmpty())
			return;

		if (m_iPollMs <= 0)
			m_iPollMs = 500;

		if (!m_bWatching)
		{
			m_bWatching = true;
			// Use the Scenario Framework's non-pausable queue so it ticks reliably in preview
			SCR_ScenarioFrameworkCallQueueSystem.GetCallQueueNonPausable().CallLater(CheckCompletion, 10, true);
		}
	}

	//! DFS over THIS entity's subtree; keep only SCR_ScenarioFrameworkSlotDestroy.
	protected void GatherDestroySlotsInSubtree(IEntity node, notnull array<SCR_ScenarioFrameworkSlotTask> outSlots)
	{
		if (!node) return;

		array<Managed> comps = {};
		node.FindComponents(SCR_ScenarioFrameworkSlotTask, comps); // IEntity.FindComponents
		foreach (Managed m : comps)
		{
			SCR_ScenarioFrameworkSlotTask s = SCR_ScenarioFrameworkSlotTask.Cast(m);
			if (!s) continue;

			SCR_ScenarioFrameworkSlotDestroy d = SCR_ScenarioFrameworkSlotDestroy.Cast(s);
			if (d) outSlots.Insert(s);
		}

		IEntity child = node.GetChildren();
		while (child)
		{
			GatherDestroySlotsInSubtree(child, outSlots);
			child = child.GetSibling();
		}
	}

	//! Periodically check progress; finish the underlying Task when all slots are FINISHED.
	protected void CheckCompletion()
	{
		int total = m_DestroySlots.Count();
		if (total <= 0) return;

		int finished = 0;
		foreach (SCR_ScenarioFrameworkSlotTask s : m_DestroySlots)
		{
			if (!s) continue;
			if (s.GetTaskState() == SCR_TaskState.FINISHED) // SlotTask state query
				finished++;
		}

		if (m_bDebug && finished != m_iLastFinished)
		{
			m_iLastFinished = finished;
			Print(string.Format("[CAV_DestroyAll] Progress: %1 / %2 destroy-slots finished", finished, total), LogLevel.NORMAL);
		}

		if (finished == total)
		{
			SCR_ScenarioFrameworkCallQueueSystem.GetCallQueueNonPausable().Remove(CheckCompletion);
			m_bWatching = false;

			if (m_bDebug)
				Print("[CAV_DestroyAll] All destroy-slots finished -> Finishing task", LogLevel.NORMAL);

			SCR_ScenarioFrameworkTask t = GetTask();
			if (t) t.Finish(); // mark task complete
		}
	}

	void ~CAV_LayerTaskDestroyAll()
	{
		if (m_bWatching)
			SCR_ScenarioFrameworkCallQueueSystem.GetCallQueueNonPausable().Remove(CheckCompletion);
	}
}
