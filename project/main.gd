extends Control

# load the SIMPLE library
const GDDL = preload("res://gdnative/gddl.gdns")
onready var data = GDDL.new()

func _on_Button_pressed():
	data.download_file("https://crouton.net", "/tmp/crouton.html")
