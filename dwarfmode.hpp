struct dwarfmode_hook : public df::viewscreen_dwarfmodest
{
    typedef df::viewscreen_dwarfmodest interpose_base;

    int get_menu_width()
    {
        uint8_t menu_width, area_map_width;
        Gui::getMenuWidth(menu_width, area_map_width);
        int32_t menu_w = 0;

        bool menuforced = (ui->main.mode != df::ui_sidebar_mode::Default || df::global::cursor->x != -30000);

        if ((menuforced || menu_width == 1) && area_map_width == 2) // Menu + area map
            menu_w = 55;
        else if (menu_width == 2 && area_map_width == 2) // Area map only
        {
            menu_w = 24;
        }
        else if (menu_width == 1) // Wide menu
            menu_w = 55;
        else if (menuforced || (menu_width == 2 && area_map_width == 3)) // Menu only
            menu_w = 31;

        return menu_w;
    }

    DEFINE_VMETHOD_INTERPOSE(void, feed, (std::set<df::interface_key> *input))
    {
        renderer_cool *r = (renderer_cool*)enabler->renderer;

        init->display.grid_x = r->gdimxfull + gmenu_w + 2;
        init->display.grid_y = r->gdimyfull + 2;

        INTERPOSE_NEXT(feed)(input);

        init->display.grid_x = tdimx;
        init->display.grid_y = tdimy;

        int menu_w_new = get_menu_width();
        if (gmenu_w != menu_w_new)
        {
            gmenu_w = menu_w_new;
            r->needs_reshape = true;
        }
    }

    DEFINE_VMETHOD_INTERPOSE(void, logic, ())
    {
        INTERPOSE_NEXT(logic)();

        renderer_cool *r = (renderer_cool*)enabler->renderer;

        if (df::global::ui->follow_unit != -1)
        {
            df::unit *u = df::unit::find(df::global::ui->follow_unit);
            if (u)
            {
                *df::global::window_x = std::max(0, std::min(world->map.x_count - r->gdimxfull, u->pos.x - r->gdimx / 2));
                *df::global::window_y = std::max(0, std::min(world->map.y_count - r->gdimyfull, u->pos.y - r->gdimy / 2));
            }
        }
    }

    DEFINE_VMETHOD_INTERPOSE(void, render, ()) //Here it hooks into DF's default renderer (that it zeroes)
    {
        //clock_t c1 = clock();
        screen_map_type = 1;

        renderer_cool *r = (renderer_cool*)enabler->renderer;

        if (gmenu_w < 0)
        {
            gmenu_w = get_menu_width();
            r->needs_reshape = true;
        }

        r->reshape_zoom_swap();

        memset(gscreen_under, 0, r->gdimx*r->gdimy*sizeof(uint32_t)); //Add new layers for us to use
        memset(gscreen_building, 0, r->gdimx*r->gdimy*sizeof(uint32_t));
        #ifdef unittracking 
			memset(unit_tracker, 0, 1+ceil((r->gdimx*r->gdimy*sizeof(uint8_t)*0.125)));
		#endif
        screen_under_ptr = gscreen_under;
        screen_building_ptr = gscreen_building;
        screen_ptr = gscreen; //Add a buffer to access it
        mwindow_x = gwindow_x; //multilevel
        
        /*
			//Reset/set values
        if (last_advmode_tick != *df::global::cur_year_tick_advmode){
        last_advmode_tick = *df::global::cur_year_tick_advmode;
			set_vermin_positions();	
			vermin_position_map.clear();
			vermin_position_map.reserve(world->vermin.all.size());
        }
        */
			//Prep for about 1/16 tiles in view being buildings, which is probably excessive.
		const int mapsize = (r->gdimx*r->gdimy >> 4) * sizeof(bool);
        painted_building_map.clear();
        painted_building_map.reserve(mapsize);
        buildingbuffer_map.clear();
        buildingbuffer_map.reserve(mapsize);
        
        drawn_item_map.clear();
        drawn_item_map.reserve(mapsize);
         /* item_tile_map stores item id - tile number pairs. 
			In most cases the relation is constant and thus doesn't need to be cleared
			A melted item changing _type becomes a new item.
			However, in cases of rot timer, the item has flag set while keeping old item_id
        */
        item_tile_map.clear(); 
        item_tile_map.reserve(mapsize);
        
        made_units_transparent=false;

        INTERPOSE_NEXT(render)();

#ifdef WIN32
        static bool patched = false;
        if (!patched)
        {
            MemoryPatcher p(Core::getInstance().p);
            apply_patch(&p, p_dwarfmode_render);
            patched = true;
            //*out2 << *(int*)p_dwarfmode_render.addr << std::endl;
        }
#endif

        // These values may change from the main thread while being accessed from the rendering thread,
        // and that will cause flickering of overridden tiles at least, so save them here
        gwindow_x = *df::global::window_x;
        mwindow_x = gwindow_x; //this also got offset in interpose, so need to rerecord
        gwindow_y = *df::global::window_y;
        gwindow_z = *df::global::window_z;
		uint32_t *z = (uint32_t*)gscreen;
		
				
		// Zeroing out values to the right and below where map ends, if there's still any tiles to render there.
        if((world->map.x_count-*df::global::window_x) < r->gdimx)
        for (int x = world->map.x_count-*df::global::window_x; x < r->gdimx; x++)
        {
            for (int y = 0; y < r->gdimy; y++)
            {
                z[x*r->gdimy+y] = 0;
                gscreentexpos[x*r->gdimy+y] = 0;
            }
        }
        if((world->map.y_count-*df::global::window_y) < r->gdimy)
        for (int x = 0; x < r->gdimx; x++)
        {
            for (int y = world->map.y_count-*df::global::window_y; y < r->gdimy; y++)
            {
                z[x*r->gdimy+y] = 0;
                gscreentexpos[x*r->gdimy+y] = 0;
            }
        }

        uint8_t *sctop                     = gps->screen;
        int32_t *screentexpostop              = gps->screentexpos;
        int8_t *screentexpos_addcolortop   = gps->screentexpos_addcolor;
        uint8_t *screentexpos_grayscaletop = gps->screentexpos_grayscale;
        uint8_t *screentexpos_cftop        = gps->screentexpos_cf;
        uint8_t *screentexpos_cbrtop       = gps->screentexpos_cbr;

        // In fort mode render_map() will render starting at (1,1)
        // and will use dimensions from init->display.grid to calculate map region to render
        // but dimensions from gps to calculate offsets into screen buffer.
        // So we adjust all this so that it renders to our gdimx x gdimy buffer starting at (0,0).
        gps->screen                 = gscreen                 - 4*r->gdimy - 4;
        gps->screen_limit           = gscreen                 + r->gdimx * r->gdimy * 4;
        gps->screentexpos           = gscreentexpos           - r->gdimy - 1;//graphic ids for creatures, otherwise 0s in VBO
        gps->screentexpos_addcolor  = gscreentexpos_addcolor  - r->gdimy - 1;
        gps->screentexpos_grayscale = gscreentexpos_grayscale - r->gdimy - 1;
        gps->screentexpos_cf        = gscreentexpos_cf        - r->gdimy - 1;
        gps->screentexpos_cbr       = gscreentexpos_cbr       - r->gdimy - 1;

        init->display.grid_x = r->gdimx + gmenu_w + 2;
        init->display.grid_y = r->gdimy + 2;
        gps->dimx = r->gdimx;
        gps->dimy = r->gdimy;
        gps->clipx[1] = r->gdimx;
        gps->clipy[1] = r->gdimy;

        if (maxlevels)
            patch_rendering(false);

        //if I record unit pos at this point, it fails even with redraw
        
		// Before zeroing out values, record (old) unit locations.
		#ifdef unittracking 
        for (int t = 0; t < r->gdimx*r->gdimy; t++)
        {
			if(gscreentexpos[t])
				unit_tracker[t>>3] |= (1 <<(t&7)); //bitmap saving for bool
		} 
		#endif
		//only works in redraw, even with _old, for some reason?
	
	/*	
	int ttexpos = 0;
    int gtexpos = 0;
    int otexpos = 0;
    int firstx = 0;
    int firsty = 0;
    for (int x =0; x < r->gdimx; x++)
    for (int y = 0; y < r->gdimy; y++)
    {
		if(gps->screentexpos[x*r->gdimy+y])//0 on maximizing
			ttexpos= 1;
		if(gscreentexpos[x*r->gdimy+y]){//0 on maximizing
			if(!gtexpos){firstx=x; firsty=y;}
			gtexpos= 1;}
		if(gscreentexpos_old[x*r->gdimy+y])//1 on maximizing
			otexpos= 1;
	}if(well_info_count>0){well_info_count--; 
		*out2 << "TWBT detects gps " << ttexpos << " general " << gtexpos << " old " << otexpos << " firstgx " << firstx << " firstgy " << firsty << " unit tracker there " << ((unit_tracker[(firstx*r->gdimy+firsty) >> 3] & (1 << ((firstx*r->gdimy+firsty) & 7))) ? " present " : " absent ") << std::endl;
		}*/

    render_map();
        
        if (maxlevels)
        {
            multi_rendered = false;

            gps->screen                 = mscreen                 - 4*r->gdimy - 4;
            gps->screen_limit           = mscreen                 + r->gdimx * r->gdimy * 4;
            gps->screentexpos           = mscreentexpos           - r->gdimy - 1;
            gps->screentexpos_addcolor  = mscreentexpos_addcolor  - r->gdimy - 1;
            gps->screentexpos_grayscale = mscreentexpos_grayscale - r->gdimy - 1;
            gps->screentexpos_cf        = mscreentexpos_cf        - r->gdimy - 1;
            gps->screentexpos_cbr       = mscreentexpos_cbr       - r->gdimy - 1;

            
            screen_under_ptr = mscreen_under;
            screen_building_ptr = mscreen_building;
            screen_ptr = mscreen;

            bool lower_level_rendered = false;
            int curdepth = 1;
            //int curdepth_g = 1;
            //int curdepth_b = 1;
            //int curdepth_u = 1;
            int x0 = 0;
            int zz0 = *df::global::window_z; // Current "top" zlevel
            int maxp = std::min(maxlevels, zz0);

            do
            {
                if (curdepth == maxlevels)
                    patch_rendering(true);

                (*df::global::window_z)--;

                lower_level_rendered = false;
                int x00 = x0;
                int zz = zz0 - curdepth + 1; // Last rendered zlevel in gscreen, the tiles of which we're checking below


                int x1 = std::min(r->gdimx, world->map.x_count-*df::global::window_x);
                int y1 = std::min(r->gdimy, world->map.y_count-*df::global::window_y);
                int gscr_z = 0;
                for (int x = x0; x < x1; x++)
                {
                    for (int y = 0; y < y1; y++)
                    {
                        const int tile = x * r->gdimy + y, stile = tile * 4;

                        unsigned char ch = gscreen[stile+0];

                        int xx = *df::global::window_x + x;
                        int yy = *df::global::window_y + y;
                        if (xx < 0 || yy < 0)
                            continue;
                        
                        int xxquot = xx >> 4, xxrem = xx & 15;
                        int yyquot = yy >> 4, yyrem = yy & 15;
							df::map_block *block0 = world->map.block_index[xxquot][yyquot][zz];//zz0 is better to prevent shadows
                        if (!block0) continue; //don't look below tiles that lack blocks (though that can have issues with very high skyz I guess)
                        
                        bool not_openspace = block0->tiletype[xxrem][yyrem] != df::tiletype::OpenSpace;
                        bool not_ramptop = block0->tiletype[xxrem][yyrem] != df::tiletype::RampTop;
                        // Continue if the tile is not empty and doesn't look like a downward ramp
                        //TODO check: What if I don't continue if tile has transparent parts i.e. floating grate, axle, windmill, etc.
							//Hm, maybe I'd need to switch overlay to underlay for vermin, so that I can add the stuff below in grayscale
							//Not just overlay isn't empty - the map and item layers are also empty (or sky)
                        //if (ch != 0 && ch != 31)	//TODO: make it check for empty space tiletype, as below
                          //  continue;
                          
                        if (not_openspace && not_ramptop)
							continue;


                        // If the tile looks like a downward ramp, check that it's really a ramp
                        // Also, no need to go deeper if the ramp is covered with water
                        if (ch == 31 || gscreen_building[stile+0] == 31 || gscreen_under[stile+0] == 31)
                        {
                            //TODO: zz0 or zz ??
                            if (not_ramptop || block0->designation[xxrem][yyrem].bits.flow_size)
                                continue;
                        }
                        
							
							//open air with something floating that we want to keep
						bool pass_gscreen_building = (0 == gscreen_building[stile+0]) || (!not_ramptop && 31 == gscreen_building[stile+0]) || (raw_tile_of_top_vermin(xx,yy,zz)==gscreen_building[stile+0]);
							//TODO add ignore vermin check
							//which means passing to below when it is just vermin in openspace
							//and not vermin/building or vermin/item or vermin/creature
						bool pass_gscreen = (ch == 0) || (!not_ramptop && 31 == ch) || (raw_tile_of_top_vermin(xx,yy,zz)==ch && !pass_gscreen_building); 
						bool pass_gscreen_under = (0 == gscreen_under[stile+0]) || (!not_ramptop && 31 == gscreen_under[stile+0]);
						
						// originally intended to prevent the issue with reindeer
						//unfortunately, anti-blank system means floating building over floating building results in terrain being not taken from below
						//could add a g_u!=g_b check - in that case, with just 1 item, the terrain in g_u remains the topmost terrain, and zz replacing the bottom matters not
						/*
						if((*((uint32_t*)gscreen_under + tile)) != (*((uint32_t*)gscreen_building + tile)))
						  if(!pass_gscreen_building && !pass_gscreen && 
						  !((gscreen_under[stile+0] == 32) || (gscreen_under[stile+0] == 0) || (!not_ramptop && 31 == gscreen_under[stile+0]))){
													if ( (xx==28) && (yy==9) && (well_info_count>0)) {
							*out2 << " g_u is " << (int32_t)gscreen_under << " char is " << (int32_t)(gscreen_under[stile+0])<< std::endl;
							 well_info_count--;
							}
							 continue;} 
							 */
							 
							//continue if g_u was set to non-skip tile at any point
							//To prevent the replacement from happening with 2z of openspace below wall
							
						//Ahh, figured out the reason why some transparency, including vermin ones, fail:
						// vermin z101 is vermin/terrain/terrain
						// nothing z100 is terrain/nothing/nothing
						// result: vermin/nothing/nothing
						// same case for floating axles and such
						
                        // If the tile is empty, render the next zlevel (if not rendered already)
                        if (!lower_level_rendered)
                        {
                            multi_rendered = true;

                            // All tiles to the left were not empty, so skip them
                            x0 = x;
                            
                            (*df::global::window_x) += x0;
                            init->display.grid_x -= x0;
                            mwindow_x = gwindow_x + x0;
                            made_units_transparent = false; //otherwise vermin transparency fails on terrain on lower levels.

                            memset(mscreen_under, 0, (r->gdimx-x0)*r->gdimy*sizeof(uint32_t));
                            memset(mscreen_building, 0, (r->gdimx-x0)*r->gdimy*sizeof(uint32_t));
                            render_map(); //renders the map 1z below zz due the -- at start i.e. m is 1z below g

                            (*df::global::window_x) -= x0;
                            init->display.grid_x += x0;

                            x00 = x0;

                            lower_level_rendered = true;
                        }
                        
						if(!pass_gscreen && !pass_gscreen_building && !pass_gscreen_under) continue;

                        const int tile2 = (x-(x00)) * r->gdimy + y;//, stile2 = tile2 * 4;

                        if(pass_gscreen){
							*((uint32_t*)gscreen + tile) = *((uint32_t*)mscreen + tile2); //here I set the actual layer I use to use the mscreen rendering result if empty/ramp
							gscreen[stile+3] = (0x10*curdepth) | (gscreen[stile+3]&0x0f);
						}
						
						gscr_z = ((zz-1)+(curdepth)-((gscreen[stile+3] & 0xf0) >> 4));
						
                        if(pass_gscreen_building){
							if(!pass_gscreen && //support for transparent floating buildings in case of gscreen having the building and mscreen having something below the building
							     !((drawn_item_map.find(xx*mapy*mapzb+yy*mapzb+zz-1)!=drawn_item_map.end() //Can only handle one item, so don't add support if there's item on currently rendered layer
							     && (drawn_item_map.find(xx*mapy*mapzb+yy*mapzb+gscr_z)!=drawn_item_map.end())) )){ //and gscreen layer
								*((uint32_t*)gscreen_building + tile) = *((uint32_t*)mscreen + tile2);
								gscreen_building[stile+3] = (0x10*curdepth) | (gscreen_building[stile+3]&0x0f);//_g
							} else {
							*((uint32_t*)gscreen_building + tile) = *((uint32_t*)mscreen_building + tile2);
							gscreen_building[stile+3] = (0x10*curdepth) | (gscreen_building[stile+3]&0x0f);//_b
							}
						}
						//if blank, it's only terrain on tile, as item/bld/vermin/creature all set it 
						Infomin(!(*((uint32_t*)mscreen_under + tile2)), "overwrote " << (*((uint32_t*)gscreen_under + tile)) << " with 0s at x " << xx << " y " << yy << " z " << zz);
						
						if(pass_gscreen_under){
							*((uint32_t*)gscreen_under + tile) = (*((uint32_t*)mscreen_under + tile2));//(mscreen_under[tile2*4]) ? (*((uint32_t*)mscreen_under + tile2)) : (*((uint32_t*)mscreen + tile2));
							gscreen_under[stile+3] = (0x10*curdepth) | (gscreen_under[stile+3]&0x0f);
						}
                        //TODO if I had building on tile, pass it to layer under instead of layer
                        
                        if (pass_gscreen && *(mscreentexpos+tile2)) //fix for the bug where creatures would look dead from z-level above
                        {
                            *(gscreentexpos + tile) = *(mscreentexpos + tile2);
                            *(gscreentexpos_addcolor + tile) = *(mscreentexpos_addcolor + tile2);
                            *(gscreentexpos_grayscale + tile) = *(mscreentexpos_grayscale + tile2);
                            *(gscreentexpos_cf + tile) = *(mscreentexpos_cf + tile2);
                            *(gscreentexpos_cbr + tile) = *(mscreentexpos_cbr + tile2);
                        }
                        
                        //Swap for w3 and bld, fixes units under glass briges and floating axles
							/*
						if ( (xx==43) && (yy==39) && (well_info_count>0)) {
							*out2 << "zz is " << ((int32_t)zz) << " pass_gsreen is " << ((pass_gscreen) ? "true" : "false") << \
							 " pass_gsreen_building is " << ((pass_gscreen_building) ? "true" : "false") << \
							 " gscreen depth is " << ((int32_t)((gscreen[tile * 4 + 3] & 0xf0) >> 4)) << \
							 " gscreen_bld depth is " << ((int32_t)((gscreen_building[tile * 4 + 3] & 0xf0) >> 4)) << " < " << ((((gscreen[tile * 4 + 3] & 0xf0) >> 4)<((gscreen_building[tile * 4 + 3] & 0xf0) >> 4)) ? "passes" : "fails") << \
							 " gscreentexpos is  " << ((*(gscreentexpos+tile)) ? "present" : "zero") << \
							 " mscreentexpos is  " << ((*(mscreentexpos+tile2)) ? "present" : "zero") << \
							 " item is " << ((drawn_item_map.find(xx*mapy*mapzb+yy*mapzb+gscr_z)==drawn_item_map.end()) ? "not present" : "present") << \
							 " building is " << ((painted_building_map.find(xx*mapy*mapzb+yy*mapzb+gscr_z)!=painted_building_map.end()) ? "present" : "not present") << std::endl;
							 well_info_count--;
						}*/
						
                        if (!pass_gscreen && pass_gscreen_building) // && !((0 == gscreen_building[stile+0]) || (!not_ramptop && 31 == gscreen_building[stile+0]))) //both gscreen and gscreen_bld have something (and we wont go deeper)
                         if (((gscreen[tile * 4 + 3] & 0xf0))<((gscreen_building[tile * 4 + 3] & 0xf0))) //and bld is 1+z below gscr
                         if (!(*(gscreentexpos+tile))) //and gscr doesn't have unit NOTE if pass_gscreen, then this and below are never true simultaneously
                          if ((*(mscreentexpos+tile2))|| //and bld does
                          ((drawn_item_map.find(xx*mapy*mapzb+yy*mapzb+gscr_z)==drawn_item_map.end())&&(drawn_item_map.find(xx*mapy*mapzb+yy*mapzb+zz-1)!=drawn_item_map.end()))) 
                          //or gscr lacks item and bld doesn't
                           if(!(ch==88 && df::global::cursor->z == gscr_z && df::global::cursor->x == xx && df::global::cursor->y == yy))
							//and it isn't a cursor (interferes with redraw_all 0)
                           //hang on, doesn't this support multiple swaps with floating items one above another?
                           //if (drawn_item_map.find(xx*mapy*mapzb+yy*mapzb+gscr_z)==drawn_item_map.end()) //and gscr doesn't have item either
                            if (buildingbuffer_map.find(xx*mapy*mapzb+yy*mapzb+gscr_z)!=buildingbuffer_map.end() 
								//and there is either building on gscr_z I want to punt to write_tile_map_building
								){
									//and gscr has building ERROR TODO FIX zz refers to gzcreen_building, not gscreen z..or not even that?
								//don't need to check for terrain since that was covered earlier by not_openspace && not_ramptop tiletype check
							//if((xx==10) && (yy==27) && well_info_count>0) *out2 << "swapping gscreen " << *((uint32_t*)gscreen + tile) << " gscreen_building " << *((uint32_t*)gscreen_building + tile) << std::endl;
							swap(*((uint32_t*)gscreen + tile), *((uint32_t*)gscreen_building + tile));
							//if((xx==10) && (yy==27) && well_info_count>0) *out2 << "swapped gscreen " << *((uint32_t*)gscreen + tile) << " gscreen_building " << *((uint32_t*)gscreen_building + tile) << "swap check is " << ((((gscreen[tile * 4 + 3] & 0xf0) >> 4)>((gscreen_building[tile * 4 + 3] & 0xf0) >> 4)) ? " true" : " false") << std::endl;
							//yup, reindeer gets a swap
							//painted_vermin_map is not an issue here because z MUST differ
							
                            *(gscreentexpos + tile) = *(mscreentexpos + tile2);
                            *(gscreentexpos_addcolor + tile) = *(mscreentexpos_addcolor + tile2);
                            *(gscreentexpos_grayscale + tile) = *(mscreentexpos_grayscale + tile2);
                            *(gscreentexpos_cf + tile) = *(mscreentexpos_cf + tile2);
                            *(gscreentexpos_cbr + tile) = *(mscreentexpos_cbr + tile2);
                            
						} else if (raw_tile_of_top_vermin(xx,yy,gscr_z)==ch 
								&& buildingbuffer_map.find(xx*mapy*mapzb+yy*mapzb+gscr_z)==buildingbuffer_map.end()) {
							//or a vermin on gscreen without building data to pull
							swap(*((uint32_t*)gscreen + tile), *((uint32_t*)gscreen_building + tile));
								//gscreen_building[stile+0]=32;
								//Problem: I need to set this to transparent texpos value
								//I could pretend there's a building with all values transparent there...
								//If it wasn't for write_tile_arrays_building firing before in swapped cases, which means used_map_overlay never comes into play
								
							//*((uint32_t*)gscreen_building + tile) = *((uint32_t*)gscreen_under + tile); //Fixes it to use value under it
							//What about z1 vermin z2 unit z3 terrain? Taking address would work better, expect that blows the vermin check
							
                            *(gscreentexpos + tile) = *(mscreentexpos + tile2);
                            *(gscreentexpos_addcolor + tile) = *(mscreentexpos_addcolor + tile2);
                            *(gscreentexpos_grayscale + tile) = *(mscreentexpos_grayscale + tile2);
                            *(gscreentexpos_cf + tile) = *(mscreentexpos_cf + tile2);
                            *(gscreentexpos_cbr + tile) = *(mscreentexpos_cbr + tile2);
						}

                    }

                }
				//curdepth_g++;//What's the point of having 3 separate toggles if I could have used just regular curdepth?
				//curdepth_b++;
				//curdepth_u++;
                if (curdepth++ >= maxp)
                    break;
            } while(lower_level_rendered);

            (*df::global::window_z) = zz0;
        }

        init->display.grid_x = gps->dimx = tdimx;
        init->display.grid_y = gps->dimy = tdimy;
        gps->clipx[1] = gps->dimx - 1;
        gps->clipy[1] = gps->dimy - 1;

        gps->screen                 = sctop;
        gps->screen_limit           = gps->screen + gps->dimx * gps->dimy * 4;
        gps->screentexpos           = screentexpostop;
        gps->screentexpos_addcolor  = screentexpos_addcolortop;
        gps->screentexpos_grayscale = screentexpos_grayscaletop;
        gps->screentexpos_cf        = screentexpos_cftop;
        gps->screentexpos_cbr       = screentexpos_cbrtop;

        //clock_t c2 = clock();
        //*out2 << (c2-c1) << std::endl;

        if (block_index_size != world->map.x_count_block*world->map.y_count_block*world->map.z_count_block)
        {
            free(my_block_index);
            mapxb = world->map.x_count_block;
            mapyb = world->map.y_count_block;
            mapzb = world->map.z_count_block;
            mapx = world->map.x_count;
            mapy = world->map.y_count;
            block_index_size = mapxb*mapyb*mapzb;
            my_block_index = (df::map_block**)malloc(block_index_size*sizeof(void*));
            //*out2 << "TWBT mapx " << world->map.x_count_block << " mapxb" << mapxb << std::endl;

            for (int x = 0; x < mapxb; x++)
            {
                for (int y = 0; y < mapyb; y++)
                {
                    for (int z = 0; z < mapzb; z++)
                    {
                        my_block_index[x*mapyb*mapzb + y*mapzb + z] = world->map.block_index[x][y][z];
                    }
                }
            }
        }
    }
};

IMPLEMENT_VMETHOD_INTERPOSE_PRIO(dwarfmode_hook, render, -200);
IMPLEMENT_VMETHOD_INTERPOSE(dwarfmode_hook, feed);
IMPLEMENT_VMETHOD_INTERPOSE(dwarfmode_hook, logic);
