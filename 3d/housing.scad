// ==========================================
// ESP32-C6 Housing v25 (Deep Screw Recess)
// ==========================================

mode = "housing"; // "housing" or "calibration"
render_part = "both"; 

// --- Dimensions & Settings ---
pcb_width = 19.5; 
pcb_length = 36.5;
pcb_thickness = 1.6;

// LCD Window
screen_real_w = 19.5;
screen_real_l = 36.5;
screen_tolerance = 0.4; 
screen_cut_w = screen_real_w + screen_tolerance;
screen_cut_l = screen_real_l + screen_tolerance;

// MX Switch
switch_body = 14.0; 
switch_tab_w = 4.5; 
switch_tab_d = 0.8; 
switch_plate_target_t = 1.5; 

// Fit Settings
fit_tolerance = 0.25; 
lid_insert_depth = 2.5; 
lid_t = 2.0;

// Case Walls
wall_t = 2.0;
screen_border_t = 1.2;

// --- Components ---
usb_w = 9.6; 
usb_h = 3.8;
usb_z_offset = -0.8; 

// Mounting Holes (Asymmetric)
hole_dist_y = 32.0; 
hole_dist_x_bottom = 16.0; 
hole_dist_x_top = 13.0; 

// [수정됨] 나사 체결부 깊이 조정
screw_pass_dia = 2.4; 
floor_support_t = 0.8; // 나사를 잡아주는 바닥 플라스틱 두께
standoff_h = 4.1; 

// [수정됨] 바닥 전체 두께 증가 (2.5 -> 3.0)
// 깊이(recess) = 3.0 - 0.8 = 2.2mm (M2 머리가 충분히 숨겨짐)
bottom_t = 3.0; 

btn_w = 4.2; 
btn_h = 2.2; 
btn_dist_from_pcb = 6.5; 
btn_z_lift = 0.5; 

corner_r = 2.0;
$fn = 60;

// --- [CRITICAL] Height Calculation ---
// PCB 높이가 바닥 두께 증가분만큼 올라감
pcb_z = bottom_t + standoff_h;

// USB 상단 높이 계산
usb_top_z = (pcb_z + (usb_h/2) + usb_z_offset) + (usb_h/2);

// 뚜껑 하단은 USB 상단보다 1.0mm 높아야 함
required_lid_bottom_z = usb_top_z + 1.0;

// 전체 높이 자동 계산
total_h = required_lid_bottom_z + lid_insert_depth;

// --- Symmetry Calculations ---
inner_w = pcb_width + (screen_border_t * 2);
outer_w = inner_w + wall_t*2;
symmetric_margin = (outer_w - switch_body) / 2;

bottom_stack = wall_t + screen_border_t;
screen_top_abs_y = bottom_stack + screen_real_l;
switch_bottom_abs_y = screen_top_abs_y + symmetric_margin;
outer_l = switch_bottom_abs_y + switch_body + symmetric_margin;
inner_l = outer_l - wall_t*2;


// --- Rendering Logic ---

if (mode == "calibration") {
    calibration_plate();
} else {
    if (render_part == "both" || render_part == "bottom") bottom_case_assembled();
    if (render_part == "both" || render_part == "top") {
        x_offset = (render_part == "both") ? outer_w + 10 : 0;
        translate([x_offset, 0, 0]) top_lid();
    }
}

// --- Modules ---

module rounded_rect(w, l, h, r) {
    linear_extrude(height=h) hull() {
        translate([r, r]) circle(r=r);
        translate([w-r, r]) circle(r=r);
        translate([w-r, l-r]) circle(r=r);
        translate([r, l-r]) circle(r=r);
    }
}

module usb_slot_cutout(w, h, depth) {
    r = h / 2; dist = w - h; 
    rotate([-90, 0, 0]) hull() {
        translate([-dist/2, 0, 0]) cylinder(h=depth, r=r);
        translate([dist/2, 0, 0]) cylinder(h=depth, r=r);
    }
}

module button_slot_cutout(w, h, depth) {
    r = h / 2; dist = w - h;
    rotate([0, 90, 0]) hull() {
        translate([0, -dist/2, 0]) cylinder(h=depth, r=r);
        translate([0, dist/2, 0]) cylinder(h=depth, r=r);
    }
}

module mx_switch_cutout(thickness) {
    notch_x_pos = (switch_body - switch_tab_w) / 2;
    union() {
        cube([switch_body, switch_body, thickness + 0.1]);
        translate([notch_x_pos, -switch_tab_d, 0])
            cube([switch_tab_w, switch_tab_d, thickness + 0.1]);
        translate([notch_x_pos, switch_body, 0])
            cube([switch_tab_w, switch_tab_d, thickness + 0.1]);
    }
}

module standoff_positions(is_hole=false) {
    center_x = wall_t + screen_border_t + pcb_width/2;
    center_y = wall_t + screen_border_t + pcb_length/2;
    
    dy = hole_dist_y / 2;
    dx_bot = hole_dist_x_bottom / 2;
    dx_top = hole_dist_x_top / 2;
    
    points = [
        [-dx_bot, -dy], [dx_bot, -dy], 
        [-dx_top, dy],  [dx_top, dy]   
    ];

    translate([center_x, center_y, 0]) {
        for (p = points) {
            translate([p[0], p[1], 0]) {
                if (!is_hole) {
                    translate([0, 0, bottom_t]) cylinder(h=standoff_h, r=2.2);
                } else {
                    // [핵심 수정] 깊이 계산: 바닥두께(3.0) - 지지두께(0.8) = 2.2mm 깊이
                    recess_depth = bottom_t - floor_support_t;
                    translate([0, 0, -1]) cylinder(h=recess_depth + 1, r=4.2/2);
                    translate([0, 0, -1]) cylinder(h=total_h+5, r=screw_pass_dia/2);
                }
            }
        }
    }
}

module bottom_case_assembled() {
    difference() {
        union() {
            difference() {
                rounded_rect(outer_w, outer_l, total_h, corner_r);
                translate([wall_t, wall_t, bottom_t])
                    rounded_rect(inner_w, inner_l, total_h+1, 1);
            }
            standoff_positions(is_hole=false);
        }
        standoff_positions(is_hole=true);

        usb_r = usb_h / 2;
        translate([outer_w/2, -1, pcb_z + usb_r + usb_z_offset]) 
            usb_slot_cutout(usb_w, usb_h, wall_t + 5);

        abs_btn_y = wall_t + screen_border_t + btn_dist_from_pcb;
        btn_center_z = pcb_z + 1.5 + btn_z_lift; 
        
        translate([-1, abs_btn_y, btn_center_z])
            button_slot_cutout(btn_w, btn_h, wall_t + 4);
        translate([outer_w - wall_t - 1, abs_btn_y, btn_center_z])
            button_slot_cutout(btn_w, btn_h, wall_t + 4);
    }
}

module top_lid() {
    fit_w = inner_w - fit_tolerance;
    fit_l = inner_l - fit_tolerance;
    cap_w = outer_w;
    cap_l = outer_l;

    difference() {
        union() {
            rounded_rect(cap_w, cap_l, lid_t, corner_r);
            offset_val = wall_t + fit_tolerance/2;
            translate([offset_val, offset_val, -lid_insert_depth])
                rounded_rect(fit_w, fit_l, lid_insert_depth, 1);
        }
        
        pcb_center_x = wall_t + screen_border_t + pcb_width/2;
        pcb_center_y = wall_t + screen_border_t + pcb_length/2;
        
        translate([pcb_center_x - screen_cut_w/2, pcb_center_y - screen_cut_l/2, -lid_insert_depth - 1])
            rounded_rect(screen_cut_w, screen_cut_l, lid_t + lid_insert_depth + 2, 2.0);
            
        switch_abs_y = switch_bottom_abs_y;
        translate([cap_w/2 - switch_body/2, switch_abs_y, -lid_insert_depth - 1])
            mx_switch_cutout(lid_t + lid_insert_depth + 2);
            
        recess_start_z = lid_t - switch_plate_target_t; 
        recess_w = switch_body + 4;
        recess_l = switch_body + 4;
        translate([cap_w/2 - recess_w/2, switch_abs_y - 2, -lid_insert_depth - 0.1])
            cube([recess_w, recess_l, lid_insert_depth + recess_start_z + 0.1]);
    }
}

module calibration_plate() {
    difference() {
        translate([wall_t + screen_border_t, wall_t + screen_border_t, 0])
            cube([pcb_width, pcb_length, 1.2]);
        standoff_positions(is_hole=true);
        translate([wall_t + 2, wall_t + 10, 1.2]) 
            color("red") text("DEEP RECESS", size=2.0);
    }
}