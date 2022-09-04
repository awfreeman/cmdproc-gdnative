extends Control

# load the SIMPLE library
const GDDL = preload("res://gdnative/gddl.gdns")
onready var data = GDDL.new()

func _on_Button_pressed():
	#var dl_path = OS.get_data_dir() + '/crouton.html'
	# following should fail, this is used to test error catching
	var dl_path = '/crouton.html'
	print_debug("Downloading to:", dl_path)
	if not data.download_file("https://crouton.net", dl_path):
		print_debug(data.get_error())
	
