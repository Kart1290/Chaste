#
# Cmgui script automatically generated by Chaste
#
gfx read node init time -1
for ($i=0; $i<=1; $i++) { 
  gfx read node solution_$i time $i
}
gfx read ele region_number_1
gfx read ele region_number_2
gfx define faces egroup solution
gfx modify g_element solution lines select_on material default spectrum default selected_material default_selected;
gfx cr win

