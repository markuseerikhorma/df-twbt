command_result mapshot_cmd (color_ostream &out, std::vector <std::string> & parameters)
{
    int pcnt = parameters.size();
    if (legacy_mode)
    {
        *out2 << COLOR_RED << "This command is not supported in legacy mode" << std::endl;
        *out2 << COLOR_RESET;
        return CR_FAILURE;
    }

    if (!enabled)
        return CR_FAILURE;

//    CoreSuspender suspend;

    domapshot = 10;
    if(pcnt>=2)
    {
    std::string &param0 = parameters[0];
        if (param0 == "name" && pcnt >= 2);
          {
    std::string &param1 = parameters[1]; //probably can assign directly?
             mapshotname = param1;
          }
    }

    return CR_OK;    
}

command_result multilevel_cmd (color_ostream &out, std::vector <std::string> & parameters)
{
    if (!enabled)
        return CR_FAILURE;

//    CoreSuspender suspend;

    int pcnt = parameters.size();

    if (pcnt >= 1)
    {
        std::string &param0 = parameters[0];
        int newmaxlevels = maxlevels;

        if (param0 == "shadowcolor" && pcnt >= 5)
        {
            float c[4];
            bool ok = parse_float(parameters[1], c[0]) &&
                      parse_float(parameters[2], c[1]) &&
                      parse_float(parameters[3], c[2]) &&
                      parse_float(parameters[4], c[3]);

            if (ok)
                memcpy(shadowcolor, c, sizeof(shadowcolor));
            else
                return CR_WRONG_USAGE;
        }        

        else if (param0 == "fogcolor" && pcnt >= 4)
        {
            float c[3];
            bool ok = parse_float(parameters[1], c[0]) &&
                      parse_float(parameters[2], c[1]) &&
                      parse_float(parameters[3], c[2]);

            if (ok)
                memcpy(fogcolor, c, sizeof(fogcolor));
            else
                return CR_WRONG_USAGE;
        }

        else if (param0 == "fogdensity" && pcnt >= 2)
        {
            float val[3];
            bool ok;

            ok = parse_float(parameters[1], val[0]);
            if (pcnt >= 3)
                ok &= parse_float(parameters[2], val[1]);
            if (pcnt >= 4)
                ok &= parse_float(parameters[3], val[2]);

            if (!ok)
                return CR_WRONG_USAGE;                

            fogdensity = val[0];
            if (pcnt >= 3)
                fogstart = val[1];
            else
                fogstart = 0;
            if (pcnt >= 4)
                fogstep = val[2];
            else
                fogstep = 1;

            ((renderer_cool*)enabler->renderer)->needs_full_update = true;
        }

        else if (param0 == "more")
        {
            if (maxlevels < 15)
                newmaxlevels = maxlevels + 1;
        }
        else if (param0 == "less")
        {
            if (maxlevels > 0)
                newmaxlevels = maxlevels - 1;
        }
        else 
        {
            int val;
            if (parse_int(param0, val))
                newmaxlevels = std::max (std::min(val, 15), 0);
            else
                return CR_WRONG_USAGE;
        }

        if (newmaxlevels && !maxlevels)
        {
            if (!legacy_mode)
                patch_rendering(false);

           // ((renderer_cool*)enabler->renderer)->needs_full_update = true;
          //  Tends to leave shadows behind
        }
        else if (!newmaxlevels && maxlevels)
        {
            if (!legacy_mode)
                patch_rendering(true);

            multi_rendered = false;

            ((renderer_cool*)enabler->renderer)->needs_full_update = true;
        }

        maxlevels = newmaxlevels;            
    }
    else
        return CR_WRONG_USAGE;

    return CR_OK;    
}

command_result twbt_cmd (color_ostream &out, std::vector <std::string> & parameters)
{
    if (!enabled)
        return CR_FAILURE;

//    CoreSuspender suspend;

    int pcnt = parameters.size();

    if (pcnt >= 1)
    {
        std::string &param0 = parameters[0];

        if (param0 == "tilesize")
        {
            renderer_cool *r = (renderer_cool*) enabler->renderer;
            //std::string &param2 = parameters[1];

            if (pcnt == 1)
            {
                *out2 << "Map tile size is " << r->gdispx << "x" << r->gdispy << std::endl;
                return CR_OK;
            }

            if (parameters[1] == "bigger")
            {
                r->gdispx++;
                r->gdispy++;

                r->needs_reshape = true;
            }
            else if (parameters[1] == "smaller")
            {
                if (r->gdispx > 1 && r->gdispy > 1 && (r->gdimxfull < world->map.x_count || r->gdimyfull < world->map.y_count))
                {
                    r->gdispx--;
                    r->gdispy--;

                    r->needs_reshape = true;
                }
            }

            else if (parameters[1] == "reset")
            {
                r->gdispx = enabler->fullscreen ? small_map_dispx : large_map_dispx;
                r->gdispy = enabler->fullscreen ? small_map_dispy : large_map_dispy;

                r->needs_reshape = true;                    
            }

            else if (parameters[1][0] == '+' || parameters[1][0] == '-')
            {
                int delta;
                if (!parse_int(parameters[1], delta))
                    return CR_WRONG_USAGE;

                r->gdispx += delta;
                r->gdispy += delta;

                r->needs_reshape = true;
            }

            else if (pcnt >= 3)
            {
                int w, h;
                bool ok = parse_int(parameters[1], w) &&
                          parse_int(parameters[2], h);

                if (ok)
                {
                    r->gdispx = w;
                    r->gdispy = h;
                    r->needs_reshape = true;
                }
                else
                    return CR_WRONG_USAGE;   

            }
            else
                return CR_WRONG_USAGE;
        }

        else if (param0 == "redraw_all")
        {
            int on;
            if (!parse_int(parameters[1], on))
                return CR_WRONG_USAGE;

            always_full_update = (on > 0);
            *out2 << "Forced redraw mode " << (always_full_update ? "enabled" : "disabled") << std::endl;
        }

        else if (param0 == "hide_stockpiles")
        {
            if (!always_full_update)
            {
                *out2 << COLOR_RED << "Hiding stockpiles requires \"twbt redraw_all 1\"" << std::endl;
                *out2 << COLOR_RESET;
                return CR_WRONG_USAGE;
            }

            int on;
            if (!parse_int(parameters[1], on))
                return CR_WRONG_USAGE;

            hide_stockpiles = (on > 0);
            if (hide_stockpiles)
                *out2 << "Hiding stockpiles unless in [q], [p] or [k] mode" << std::endl;
            else
                *out2 << "Always showing stockpiles" << std::endl;
                
            enable_building_hooks();
        }
/*
        else if (param0 == "unit_transparency")
        {
            int on;
            if (!parse_int(parameters[1], on))
                return CR_WRONG_USAGE;

            unit_transparency = (on > 0);
            *out2 << "Unit transparency " << (unit_transparency ? "enabled" : "disabled") << std::endl;

            enable_unit_hooks();
            gps->force_full_display_count = 1;
            ((renderer_cool*)enabler->renderer)->needs_full_update = true;
        }
*/
        else if (param0 == "workshop_transparency")
        {
            int on;
            if (!parse_int(parameters[1], on))
                return CR_WRONG_USAGE;

            workshop_transparency = (on > 0);
            *out2 << "Workshop transparency " << (workshop_transparency ? "enabled" : "disabled") << std::endl;

            enable_building_hooks();
            gps->force_full_display_count = 1;
            ((renderer_cool*)enabler->renderer)->needs_full_update = true;
        }
        #ifdef tweeting
        else if (param0 == "debug") {
            int on;
            if (!parse_int(parameters[1], on))
                return CR_WRONG_USAGE;
            well_info_count = on;
            *out2 << "Printing debug info repeats at " << (well_info_count) << " debug count is " << ((well_info_count<0) ? "negative" : "positive") << std::endl;
		}
		#endif
    }

    return CR_OK;    
}

command_result colormap_cmd (color_ostream &out, std::vector <std::string> & parameters)
{
    if (!enabled)
        return CR_FAILURE;

    CoreSuspender suspend;

    int pcnt = parameters.size();

    if (pcnt == 1)
    {
        std::string &param0 = parameters[0];
        if (param0 == "reload")
        {
            load_colormap();
        }
        else
        {
            int cidx = color_name_to_index(param0);

            if (cidx != -1)
                out << param0 << " = " <<
                    roundf(enabler->ccolor[cidx][0] * 255) << " " <<
                    roundf(enabler->ccolor[cidx][1] * 255) << " " <<
                    roundf(enabler->ccolor[cidx][2] * 255) << std::endl;
        }

        gps->force_full_display_count = 1;
        ((renderer_cool*)enabler->renderer)->needs_full_update = true;
    }
    else if (pcnt == 4)
    {
        int cidx = color_name_to_index(parameters[0]);

        if (cidx != -1)
        {
            float c[3];
            bool ok = parse_float(parameters[1], c[0]) &&
                      parse_float(parameters[2], c[1]) &&
                      parse_float(parameters[3], c[2]);

            if (ok)
            {
                c[0] /= 255.0f, c[1] /= 255.0f, c[2] /= 255.0f;
                memcpy(enabler->ccolor[cidx], c, sizeof(enabler->ccolor[cidx]));

                gps->force_full_display_count = 1;
                ((renderer_cool*)enabler->renderer)->needs_full_update = true; 
            }
            else
                return CR_WRONG_USAGE;            
        }  
    }

    return CR_OK;    
}
