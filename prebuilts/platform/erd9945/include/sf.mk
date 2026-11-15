# SurfaceFlinger
PRODUCT_PROPERTY_OVERRIDES += \
    ro.surface_flinger.max_frame_buffer_acquired_buffers=3 \
    ro.surface_flinger.running_without_sync_framework=false \
    ro.surface_flinger.use_color_management=true\
    ro.surface_flinger.has_wide_color_display=true \
    ro.surface_flinger.has_HDR_display = 1 \
    persist.sys.sf.color_saturation=1.0 \
    debug.sf.auto_latch_unsignaled=0 \
    debug.sf.latch_unsignaled=1 \
    ro.surface_flinger.set_display_power_timer_ms=200 \

# Screen property
#PRODUCT_PROPERTY_OVERRIDES += \
#ro.sf.lcd_density=560

PRODUCT_PROPERTY_OVERRIDES += \
    ro.surface_flinger.protected_contents=1

# Set Game Default Frame Rate
# See b/286084594
#PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
#    ro.surface_flinger.game_default_frame_rate_override=60

# VRR
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += debug.sf.show_refresh_rate_overlay_render_rate=true
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.surface_flinger.game_default_frame_rate_override=60
ifeq ($(shell secgetspf SEC_PRODUCT_FEATURE_LCD_CONFIG_HFR_MODE),1)
# Switchable
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.surface_flinger.use_content_detection_for_refresh_rate=false
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.surface_flinger.enable_frame_rate_override=false
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += debug.sf.use_phase_offsets_as_durations=false
else
    ifeq ($(shell secgetspf SEC_PRODUCT_FEATURE_LCD_CONFIG_HFR_MODE),2)
    # Seamless
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.surface_flinger.use_content_detection_for_refresh_rate=true
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.surface_flinger.set_idle_timer_ms=250
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.surface_flinger.set_touch_timer_ms=300
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.surface_flinger.enable_frame_rate_override=true
    endif
    ifeq ($(shell secgetspf SEC_PRODUCT_FEATURE_LCD_CONFIG_HFR_MODE),3)
    # HW Seamless
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.surface_flinger.use_content_detection_for_refresh_rate=true
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.surface_flinger.set_idle_timer_ms=250
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.surface_flinger.set_touch_timer_ms=300
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.surface_flinger.enable_frame_rate_override=true
    endif
    
    ifneq (,$(findstring b7r,$(TARGET_PRODUCT)))
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += debug.sf.use_phase_offsets_as_durations=true
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += debug.sf.late.sf.duration=8333333
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += debug.sf.early.sf.duration=8333333
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += debug.sf.earlyGl.sf.duration=8333333
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += debug.sf.late.app.duration=8333333
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += debug.sf.early.app.duration=8333333
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += debug.sf.earlyGl.app.duration=8333333
    else
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += debug.sf.use_phase_offsets_as_durations=false
    endif
endif

# secondary_display_orientation
ifneq (,$(findstring b7r,$(TARGET_PRODUCT)))
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.surface_flinger.secondary_display_orientation=ORIENTATION_90
endif
