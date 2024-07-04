all: stl/Caster.stl stl/Wheel.stl stl/PenCollar.stl stl/Chassis.stl

stl/%.stl: %.scad
	openscad --export-format binstl -o $@ $<
