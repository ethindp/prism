@tool
extends Node


func _ready() -> void:
	var backend := Prism.create_best()
	print("Prism backend created: %s" % backend.name)
