#!/usr/bin/env python3
"""0.8.61: no-eviction NVS, immutable lyric cache, per-track rate, safe exits."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
cache=(p/"main/song_cache.c").read_text()
header=(p/"main/song_cache.h").read_text()
ui=(p/"main/ui.c").read_text()
recog=(p/"main/recognition.c").read_text()
brand=(p/"main/brand_icon.c").read_text()
cm=(p/"main/CMakeLists.txt").read_text()
version=(p/"main/version.h").read_text()
part=(p/"partitions.csv").read_text()
assert 'LF_VERSION "0.8.126-alpha"' in version
assert "0x100000" in part and "0x500000" in part
assert '"song_cache.c"' in cm and '"brand_icon.c"' in cm
assert 'LF_CACHE_NS "lf_songs"' in cache
assert "LF_CACHE_LIMIT 19000U" in cache
assert "lf_song_cache_load_lyrics(" in cache and "lf_song_cache_store_lyrics(" in cache
assert "lf_song_cache_load_profile(" in cache and "lf_song_cache_store_profile(" in cache
assert "nvs_set_blob" in cache and "nvs_get_blob" in cache
assert "nvs_commit(n)" in cache
assert "nvs_erase" not in cache and "nvs_flash_erase" not in cache
assert "from_local=lf_song_cache_load_lyrics(song.title,song.artist,parsed)" in recog
assert "lf_lrclib_lookup(" in recog and "lf_song_cache_store_lyrics(" in recog
assert "lf_song_cache_load_profile(" in ui and "lf_song_cache_store_profile(" in ui
assert "timed_on_rate_slide" in ui and "timed_on_sync_slide" in ui
assert '"SAVE CURRENT"' not in ui and 'button(timed_controls,"RESET"' in ui
assert "static void timed_profile_save_if_dirty(void)" in ui
assert "timed_profile_dirty=true;" in ui
assert "timed_profile_save_if_dirty(); /* preserve song settings before scene cleanup */" in ui
assert "lf_sync_rate_position(" in ui
assert "lv_slider_set_range(timed_rate_slider,75,125);" in ui
scene=ui.split("static void build_timed_lyrics_now(const lf_recog_snapshot_t *song)\n{",1)[1].split("static void build_preview_now",1)[0]
assert "lv_slider_set_range(timed_sync_slider,-10000,10000);" not in scene
assert "lv_obj_set_style_bg_opa(timed_controls,(lv_opa_t)38,0);" in scene
assert "const bool exiting_lyrics=timed_overlay!=NULL;" in ui
assert "if(was_pager && primary_pager){" in ui
assert "if(page==1 && timed_lyrics_mode)last_activity_ms=current;" in ui
assert "ui.ui_page==9" in (p/"main/main.c").read_text()
assert "egg_icon_src=lf_brand_icon_create(mark);" in ui
assert "LyricFlash_Icon_128.png" in brand and "lv_canvas_set_buffer(" in brand
print("PASS: persistent lyrics and song profiles, manual flash writes, tempo/sync sliders, custom original brand, UI recovery")
