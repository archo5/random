
using UnityEngine;
using UnityEditor;

public class LayoutTool : MonoBehaviour
{
	public enum ShapeType
	{
		Line,
		Rectangle,
		Circle,
		Label,
	}

	public Material material;
	public ShapeType shapeType;
	public float lineWidth = 1.0f;
	[System.NonSerialized]
	public LayoutToolText lastTextItem;
}

#if UNITY_EDITOR
[CustomEditor(typeof(LayoutTool))]
public class LayoutToolEditor : Editor
{
	public static bool editLayout = false;
	
	public static Vector3 dragStart;
	public static Vector3 dragEnd;

	public void OnSceneGUI()
	{
		if (editLayout)
		{
			int controlID = GUIUtility.GetControlID(FocusType.Passive);

			var l = (target as LayoutTool);
			var t = l.transform;

			var e = Event.current;
			Plane plane = new Plane(new Vector3(0, 0, -1), t.position);
			Ray ray = HandleUtility.GUIPointToWorldRay(Event.current.mousePosition);
			float dist;
			if (plane.Raycast(ray, out dist))
			{
				Vector3 pos = ray.GetPoint(dist);

				switch (e.type)
				{
				case EventType.MouseDown:
					if (e.button == 0)
					{
						GUIUtility.hotControl = controlID;
						dragStart = pos;
						dragEnd = pos;
						e.Use();
					}
					else if (e.button == 2)
					{
						GUIUtility.hotControl = controlID;
						var obj = HandleUtility.PickGameObject(Event.current.mousePosition, false);
						if (obj && obj.transform.parent == t)
						{
							Undo.DestroyObjectImmediate(obj);
						}
						e.Use();
					}
					break;
				case EventType.MouseUp:
					if (e.button == 0)
					{
						GameObject obj;
						switch (l.shapeType)
						{
						case LayoutTool.ShapeType.Line:
							obj = new GameObject("Line");
							obj.transform.SetParent(t, false);
							var lr = obj.AddComponent<LineRenderer>();
							var localDragStart = obj.transform.InverseTransformPoint(dragStart);
							var localDragEnd = obj.transform.InverseTransformPoint(dragEnd);
							var localCenter = (localDragStart + localDragEnd) / 2;
							obj.transform.localPosition = localCenter;
							lr.SetPositions(new Vector3[] { localDragStart - localCenter, localDragEnd - localCenter });
							lr.material = l.material;
							lr.startWidth = l.lineWidth;
							lr.endWidth = l.lineWidth;
							lr.alignment = LineAlignment.TransformZ;
							lr.generateLightingData = true;
							lr.useWorldSpace = false;
							Undo.RegisterCreatedObjectUndo(obj, "line");
							break;
						case LayoutTool.ShapeType.Rectangle:
							obj = GameObject.CreatePrimitive(PrimitiveType.Quad);
							obj.name = "Rect";
							obj.transform.SetParent(t, false);
							obj.transform.localScale = (Vector3.Max(dragStart, dragEnd) - Vector3.Min(dragStart, dragEnd)) + new Vector3(0, 0, 1);
							obj.transform.position = (dragStart + dragEnd) / 2;
							obj.GetComponent<MeshRenderer>().material = l.material;
							DestroyImmediate(obj.GetComponent<Collider>());
							Undo.RegisterCreatedObjectUndo(obj, "rect");
							break;
						case LayoutTool.ShapeType.Circle:
							obj = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
							obj.name = "Circle";
							obj.transform.SetParent(t, false);
							obj.transform.localRotation = Quaternion.Euler(90, 0, 0);
							float width = (dragStart - dragEnd).magnitude * 2;
							obj.transform.localScale = new Vector3(width, 0.01f, width);
							obj.transform.position = dragStart;
							obj.GetComponent<MeshRenderer>().material = l.material;
							DestroyImmediate(obj.GetComponent<Collider>());
							Undo.RegisterCreatedObjectUndo(obj, "circle");
							break;
						case LayoutTool.ShapeType.Label:
							obj = GameObject.CreatePrimitive(PrimitiveType.Sphere);
							obj.name = "Label";
							obj.transform.SetParent(t, false);
							obj.transform.position = dragEnd;
							obj.transform.localScale = new Vector3(0.5f, 0.5f, 0.5f);
							l.lastTextItem = obj.AddComponent<LayoutToolText>();
						//	obj.GetComponent<MeshRenderer>().material = l.material;
							DestroyImmediate(obj.GetComponent<Collider>());
							Undo.RegisterCreatedObjectUndo(obj, "label");
							break;
						}
						dragStart = Vector3.zero;
						dragEnd = Vector3.zero;
						SceneView.RepaintAll();
					}
					break;
				case EventType.MouseMove:
					SceneView.RepaintAll();
					break;
				case EventType.MouseDrag:
					if (e.button == 0)
					{
						dragEnd = pos;
						SceneView.RepaintAll();
					}
					break;
				}

				Handles.DrawWireDisc(pos, Vector3.forward, 0.5f);
				Handles.DrawWireDisc(pos, Vector3.forward, 9);
				Handles.DrawAAPolyLine(2, pos + Vector3.left * 9, pos + Vector3.right * 9);
				Handles.DrawAAPolyLine(2, pos + Vector3.up * 9, pos + Vector3.down * 9);
				var d1 = (Vector3.left + Vector3.up).normalized * 9;
				var d2 = (Vector3.right + Vector3.up).normalized * 9;
				Handles.DrawAAPolyLine(2, pos - d1, pos + d1);
				Handles.DrawAAPolyLine(2, pos - d2, pos + d2);
			}

			if (dragStart != dragEnd)
			{
				Handles.color = Color.green;
				switch (l.shapeType)
				{
				case LayoutTool.ShapeType.Line:
					Handles.DrawAAPolyLine(10.0f, dragStart, dragEnd);
					break;
				case LayoutTool.ShapeType.Rectangle:
					Handles.DrawAAConvexPolygon(
						dragStart,
						new Vector3(dragStart.x, dragEnd.y, dragStart.z),
						dragEnd,
						new Vector3(dragEnd.x, dragStart.y, dragEnd.z));
					break;
				case LayoutTool.ShapeType.Circle:
					Handles.DrawSolidDisc(dragStart, Vector3.forward, (dragStart - dragEnd).magnitude);
					break;
				case LayoutTool.ShapeType.Label:
					Handles.Label(dragEnd, "<text>", LayoutToolTextEditor.style);
					break;
				}
			}
		}
	}

	public override void OnInspectorGUI()
	{
		editLayout = GUILayout.Toggle(editLayout, "Edit", "Button");

		var l = (target as LayoutTool);

		GUILayout.BeginHorizontal();
		GUILayout.Label("Shape type");
		l.shapeType = (LayoutTool.ShapeType) GUILayout.SelectionGrid((int) l.shapeType, new string[] { "Line", "Rectangle", "Circle", "Label" }, 4);
		GUILayout.EndHorizontal();

		l.lineWidth = EditorGUILayout.Slider("Line width", l.lineWidth, 0.1f, 10.0f);

		l.material = (Material) EditorGUILayout.ObjectField("Material", l.material, typeof(Material), false);

		if (l.lastTextItem)
		{
			EditorGUILayout.LabelField("Last text item:");
			l.lastTextItem.text = EditorGUILayout.TextArea(l.lastTextItem.text);
		}
	}
}
#endif
