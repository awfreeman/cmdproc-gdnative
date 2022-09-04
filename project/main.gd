extends Control

# load the SIMPLE library
const GDDL = preload("res://gdnative/gddl.gdns")
onready var data = GDDL.new()

func _on_Button_pressed():
	var dl_path = OS.get_data_dir() + '/crouton.html'
	print_debug("Downloading to:", dl_path)
	data.download_file("https://crouton.net", dl_path)
