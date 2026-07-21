import os
import unreal


asset_path = "/Game/_Blueprints/Gameplay/Orientation/DT_OrientationFlow.DT_OrientationFlow"
output_path = os.environ.get("DOP_ORIENTATION_EXPORT", "C:/tmp/DT_OrientationFlow.csv")
table = unreal.EditorAssetLibrary.load_asset(asset_path)
if table is None:
    raise RuntimeError("Could not load {}".format(asset_path))
if not unreal.DataTableFunctionLibrary.export_data_table_to_csv_file(table, output_path):
    raise RuntimeError("Could not export {}".format(output_path))
unreal.log("Exported legacy orientation rows to {}".format(output_path))
