
using UnityEngine;
using UnityEditor;

public class LayoutToolText : MonoBehaviour
{
	[TextArea]
	public string text = "<type something here>";
}

#if UNITY_EDITOR
[CustomEditor(typeof(LayoutToolText))]
public class LayoutToolTextEditor : Editor
{
	public static GUIStyle style;
	[DrawGizmo(GizmoType.InSelectionHierarchy | GizmoType.NotInSelectionHierarchy)]
	public static void DrawHandles(LayoutToolText ltt, GizmoType gizmoType)
	{
		if (style == null)
		{
			style = new GUIStyle("WhiteMiniLabel");
		}
		Handles.Label(ltt.transform.position, ltt.text, style);
	}
}
#endif
