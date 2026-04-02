@tool
extends Node

func _ready() -> void:
	var count := Prism.get_backends_count()
	print("Prism: %d backend(s) registered" % count)
	for i in range(count):
		var bid := Prism.id_at(i)
		print("  [%d] id=%d name='%s' priority=%d" % [i, bid, Prism.name_of(bid), Prism.priority_of(bid)])
	var backend := Prism.create_best()
	if backend == null or not backend.is_valid():
		push_error("Prism: create_best() returned null: no supported backend found")
		return
	print("Prism backend created: %s" % backend.name)