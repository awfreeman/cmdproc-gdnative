extends Control

# load the SIMPLE library
const GDDL = preload("res://gdnative/gddl.gdns")
onready var dl = GDDL.new()

func _on_Button_pressed():
	var dl_path = OS.get_data_dir() + '/crouton.html'
	# following should fail, this is used to test error catching
	#var dl_path = '/crouton.html'
	
	dl.set_agent("My Useragent/1.0")
	
	print_debug("Downloading to:", dl_path)
	if not dl.download_file("https://crouton.net", dl_path):
		print_debug(dl.get_error())
		
	var ret = dl.download_to_string("https://crouton.net")
	
	if ret:
		print_debug(ret)
	else:
		print_debug(dl.get_error())
	
