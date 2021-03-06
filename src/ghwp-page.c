/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ghwp-page.c
 *
 * Copyright (C) 2012-2013 Hodong Kim <cogniti@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ghwp-page.h"

G_DEFINE_TYPE (GHWPPage, ghwp_page, G_TYPE_OBJECT);

#define _g_free0(var) (var = (g_free (var), NULL))

void ghwp_page_get_size (GHWPPage *page,
                         gdouble  *width,
                         gdouble  *height)
{
    g_return_if_fail (page != NULL);
    *width  = 595.0;
    *height = 842.0;
}

#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library ft_lib;
static FT_Face    ft_face;
/*한 번만 초기화, 로드 */
static void
once_ft_init_and_new (void)
{
    static gsize ft_init = 0;

    if (g_once_init_enter (&ft_init)) {

        FT_Init_FreeType (&ft_lib);
        FT_New_Face (ft_lib, "/usr/share/fonts/truetype/nanum/NanumGothic.ttf",
                     0, &ft_face);

        g_once_init_leave (&ft_init, (gsize)1);
    }
}

static void draw_text(cairo_t             *cr,
                      cairo_text_extents_t extents,
                      cairo_glyph_t       *glyphs,
                      cairo_scaled_font_t *scaled_font,
                      const gchar         *text,
                      double              *x,
                      double              *y)
{
    gchar *ch = NULL;
    int    num_glyphs;
    guint  j;
    for (j = 0; j < g_utf8_strlen(text, -1); j++) {
        ch = g_utf8_substring(text, j, j+1);

        cairo_scaled_font_text_to_glyphs (scaled_font, *x, *y, ch, -1,
                                          &glyphs, &num_glyphs,
                                          NULL, NULL, NULL);
        _g_free0 (ch);
        cairo_glyph_extents(cr, glyphs, num_glyphs, &extents);

        if (*x >= 595.0 - extents.x_advance - 20.0) {
            glyphs[0].x  = 20.0;
            glyphs[0].y += 16.0;
            *x  = 20.0 + extents.x_advance;
            *y += 16.0;
        }
        else {
            *x += extents.x_advance;
        }

        cairo_show_glyphs (cr, glyphs, num_glyphs);
    }

    *y += 18.0;
}

gboolean ghwp_page_render (GHWPPage *page, cairo_t *cr)
{
    g_return_val_if_fail (page != NULL, FALSE);
    g_return_val_if_fail (cr   != NULL, FALSE);
    cairo_save (cr);

    guint          i, j, k;
    GHWPParagraph *paragraph;
    GHWPText      *ghwp_text;
    GHWPTable     *table;
    GHWPTableCell *cell;

    cairo_glyph_t        *glyphs = NULL; /* NULL로 지정하면 자동 할당됨 */
    cairo_scaled_font_t  *scaled_font;
    cairo_font_face_t    *font_face;
    cairo_matrix_t        font_matrix;
    cairo_matrix_t        ctm;
    cairo_font_options_t *font_options;
    cairo_text_extents_t  extents;

    double x = 20.0;
    double y = 40.0;

    /* create scaled font */
    once_ft_init_and_new(); /*한 번만 초기화, 로드*/
    font_face = cairo_ft_font_face_create_for_ft_face (ft_face, 0);
    cairo_matrix_init_identity (&font_matrix);
    cairo_matrix_scale (&font_matrix, 12.0, 12.0);
    cairo_get_matrix (cr, &ctm);
    font_options = cairo_font_options_create ();
    cairo_get_font_options (cr, font_options);
    scaled_font = cairo_scaled_font_create (font_face,
                                           &font_matrix, &ctm, font_options);
    cairo_font_options_destroy (font_options);

    cairo_set_scaled_font(cr, scaled_font); /* 요 문장 없으면 fault 떨어짐 */
    cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);

    for (i = 0; i < page->paragraphs->len; i++) {
        paragraph = g_array_index (page->paragraphs, GHWPParagraph *, i);
        ghwp_text = paragraph->ghwp_text;
        x = 20.0;
        /* draw text */
        if ((ghwp_text != NULL) && !(g_str_equal(ghwp_text->text, "\n\r"))) {
            draw_text(cr, extents, glyphs, scaled_font, ghwp_text->text,
                      &x, &y);
        }
        /* draw table */
        table = ghwp_paragraph_get_table (paragraph);
        if (table != NULL) {
            for (j = 0; j < table->cells->len; j++) {
                cell = g_array_index(table->cells, GHWPTableCell *, j);
                for (k = 0; k < cell->paragraphs->len; k++) {
                    paragraph = g_array_index(cell->paragraphs,
                                              GHWPParagraph *, k);
                    if (paragraph->ghwp_text) {
                        /* FIXME x, y 좌표 */
                        x = 40.0 + (595.5 - 40.0) / table->n_cols * cell->col_addr;
                        if (cell->col_addr != 0)
                            y -= 16.0;
                        draw_text(cr, extents, glyphs, scaled_font,
                                  paragraph->ghwp_text->text, &x, &y);
                    }
                }
            }
        }
    }

    cairo_glyph_free (glyphs);
    cairo_scaled_font_destroy (scaled_font);

    cairo_restore (cr);
    return TRUE;
}

GHWPPage *ghwp_page_new (void)
{
    return (GHWPPage *) g_object_new (GHWP_TYPE_PAGE, NULL);
}

static void ghwp_page_finalize (GObject *obj)
{
    GHWPPage *page = GHWP_PAGE(obj);
    g_array_free (page->paragraphs, TRUE);
    G_OBJECT_CLASS (ghwp_page_parent_class)->finalize (obj);
}

static void ghwp_page_class_init (GHWPPageClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = ghwp_page_finalize;
}

static void ghwp_page_init (GHWPPage *page)
{
    page->paragraphs = g_array_new (TRUE, TRUE, sizeof (GHWPParagraph *));
}

/* experimental */
void
ghwp_page_render_selection (GHWPPage           *page,
                            cairo_t            *cr,
                            GHWPRectangle      *selection,
                            GHWPRectangle      *old_selection,
                            GHWPSelectionStyle  style, 
                            GHWPColor          *glyph_color,
                            GHWPColor          *background_color)
{
    g_return_if_fail (page != NULL);
    /* TODO */
}

/* experimental */
char *
ghwp_page_get_selected_text (GHWPPage          *page,
                             GHWPSelectionStyle style,
                             GHWPRectangle     *selection)
{
    g_return_val_if_fail (page != NULL, NULL);
    /* TODO */
    return NULL;
}

/* experimental */
GList *
ghwp_page_get_selection_region (GHWPPage          *page,
                                gdouble            scale,
                                GHWPSelectionStyle style,
                                GHWPRectangle     *selection)
{
    g_return_val_if_fail (page != NULL, NULL);
    /* TODO */
    return NULL;
}

/**
 * ghwp_rectangle_free:
 * @rectangle: a #GHWPRectangle
 *
 * Frees the given #GHWPRectangle
 */
void
ghwp_rectangle_free (GHWPRectangle *rectangle)
{
    g_return_if_fail (rectangle != NULL);
    g_slice_free (GHWPRectangle, rectangle);
}
