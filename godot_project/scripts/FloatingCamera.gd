extends Camera3D

@export var move_speed: float = 5.0
@export var fast_speed: float = 15.0
@export var sensitivity: float = 0.003

var _mouse_captured := false
var _yaw: float = 0.0
var _pitch: float = 0.0

func _ready() -> void:
	# Начальные углы из текущего поворота камеры
	_yaw   = rotation.y
	_pitch = rotation.x

func _input(event: InputEvent) -> void:
	# Зажата ЛКМ — захватываем вращение
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT:
			_mouse_captured = event.pressed

	# Вращение только пока зажата ЛКМ
	if event is InputEventMouseMotion and _mouse_captured:
		_yaw   -= event.relative.x * sensitivity
		_pitch -= event.relative.y * sensitivity
		_pitch  = clamp(_pitch, -PI / 2.0 + 0.01, PI / 2.0 - 0.01)
		rotation = Vector3(_pitch, _yaw, 0.0)

func _process(delta: float) -> void:
	if not _mouse_captured:
		return

	var speed := fast_speed if Input.is_key_pressed(KEY_SHIFT) else move_speed
	var dir   := Vector3.ZERO

	if Input.is_key_pressed(KEY_W): dir -= basis.z
	if Input.is_key_pressed(KEY_S): dir += basis.z
	if Input.is_key_pressed(KEY_A): dir -= basis.x
	if Input.is_key_pressed(KEY_D): dir += basis.x
	if Input.is_key_pressed(KEY_E): dir += Vector3.UP
	if Input.is_key_pressed(KEY_Q): dir += Vector3.DOWN

	if dir.length_squared() > 0.0:
		global_position += dir.normalized() * speed * delta
