
using System.Diagnostics;
using System.IO;
using UnityEditor;
using UnityEngine;

public class TextureCompressionTool : EditorWindow
{
	public Texture2D[] texturesToCompress;

	static long GetFileLength(string path)
	{
		var fs = File.OpenRead(path);
		long origlen = fs.Length;
		fs.Close();
		return origlen;
	}

	void OnGUI()
	{
		ScriptableObject target = this;
		SerializedObject so = new SerializedObject(target);
		SerializedProperty texturesToCompressProp = so.FindProperty("texturesToCompress");
		EditorGUILayout.PropertyField(texturesToCompressProp, true);
		so.ApplyModifiedProperties();

		if (GUILayout.Button("Convert to PNG"))
		{
			foreach (var t in texturesToCompress)
			{
				var path = AssetDatabase.GetAssetPath(t);
				var tgtpath = Path.ChangeExtension(path, "png");
				if (path == tgtpath)
				{
					UnityEngine.Debug.LogWarning($"skipping file {path} because it's already png");
					continue;
				}
				var origlen = GetFileLength(path);
				Process.Start("magick", $"convert \"{path}\" \"{tgtpath}\"").WaitForExit();
				var convlen = GetFileLength(tgtpath);
				UnityEngine.Debug.LogFormat("{3}: Original file size: {0} bytes, PNG: {1} bytes, size %: {2:0.00}", origlen, convlen, convlen * 100.0 / origlen, path);
				//if (path != tgtpath) File.Delete(tgtpath);

				File.Move(path + ".meta", tgtpath + ".meta");
				File.Delete(path);
				File.Delete(path + ".meta");
			}
		}
	}

	[MenuItem("Tools/Texture compression")]
	public static void Open()
	{
		var w = GetWindow<TextureCompressionTool>();
		w.titleContent = new GUIContent("Texture Compression Tool");
	}
}
