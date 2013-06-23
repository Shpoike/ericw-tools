/*
    Copyright (C) 1996-1997  Id Software, Inc.
    Copyright (C) 1997       Greg Lewis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/
// tjunc.c

#include "qbsp.h"

static int numwedges, numwverts;
static int tjuncs;
static int tjuncfaces;

static int cWVerts;
static int cWEdges;

static wvert_t *pWVerts;
static wedge_t *pWEdges;


//============================================================================

#define	NUM_HASH	1024

static wedge_t *wedge_hash[NUM_HASH];
static vec3_t hash_min, hash_scale;

static void
InitHash(vec3_t mins, vec3_t maxs)
{
    vec3_t size;
    vec_t volume;
    vec_t scale;
    int newsize[2];

    VectorCopy(mins, hash_min);
    VectorSubtract(maxs, mins, size);
    memset(wedge_hash, 0, sizeof(wedge_hash));

    volume = size[0] * size[1];

    scale = sqrt(volume / NUM_HASH);

    newsize[0] = (int)(size[0] / scale);
    newsize[1] = (int)(size[1] / scale);

    hash_scale[0] = newsize[0] / size[0];
    hash_scale[1] = newsize[1] / size[1];
    hash_scale[2] = (vec_t)newsize[1];
}

static unsigned
HashVec(vec3_t vec)
{
    unsigned h;

    h = (unsigned)(hash_scale[0] * (vec[0] - hash_min[0]) * hash_scale[2] +
		   hash_scale[1] * (vec[1] - hash_min[1]));
    if (h >= NUM_HASH)
	return NUM_HASH - 1;
    return h;
}

//============================================================================

static void
CanonicalVector(vec3_t vec)
{
    VectorNormalize(vec);
    if (vec[0] > EQUAL_EPSILON)
	return;
    else if (vec[0] < -EQUAL_EPSILON) {
	VectorSubtract(vec3_origin, vec, vec);
	return;
    } else
	vec[0] = 0;

    if (vec[1] > EQUAL_EPSILON)
	return;
    else if (vec[1] < -EQUAL_EPSILON) {
	VectorSubtract(vec3_origin, vec, vec);
	return;
    } else
	vec[1] = 0;

    if (vec[2] > EQUAL_EPSILON)
	return;
    else if (vec[2] < -EQUAL_EPSILON) {
	VectorSubtract(vec3_origin, vec, vec);
	return;
    } else
	vec[2] = 0;

    Error("Degenerate edge at (%.3f %.3f %.3f)", vec[0], vec[1], vec[2]);
}

static wedge_t *
FindEdge(vec3_t p1, vec3_t p2, vec_t *t1, vec_t *t2)
{
    vec3_t origin;
    vec3_t edgevec;
    wedge_t *edge;
    vec_t temp;
    int h;

    VectorSubtract(p2, p1, edgevec);
    CanonicalVector(edgevec);

    *t1 = DotProduct(p1, edgevec);
    *t2 = DotProduct(p2, edgevec);

    VectorMA(p1, -*t1, edgevec, origin);

    if (*t1 > *t2) {
	temp = *t1;
	*t1 = *t2;
	*t2 = temp;
    }

    h = HashVec(origin);

    for (edge = wedge_hash[h]; edge; edge = edge->next) {
	temp = edge->origin[0] - origin[0];
	if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
	    continue;
	temp = edge->origin[1] - origin[1];
	if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
	    continue;
	temp = edge->origin[2] - origin[2];
	if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
	    continue;

	temp = edge->dir[0] - edgevec[0];
	if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
	    continue;
	temp = edge->dir[1] - edgevec[1];
	if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
	    continue;
	temp = edge->dir[2] - edgevec[2];
	if (temp < -EQUAL_EPSILON || temp > EQUAL_EPSILON)
	    continue;

	return edge;
    }

    if (numwedges >= cWEdges)
	Error("Internal error: didn't allocate enough edges for tjuncs?");
    edge = pWEdges + numwedges;
    numwedges++;

    edge->next = wedge_hash[h];
    wedge_hash[h] = edge;

    VectorCopy(origin, edge->origin);
    VectorCopy(edgevec, edge->dir);
    edge->head.next = edge->head.prev = &edge->head;
    edge->head.t = VECT_MAX;

    return edge;
}


/*
===============
AddVert

===============
*/
static void
AddVert(wedge_t *edge, vec_t t)
{
    wvert_t *v, *newv;

    v = edge->head.next;
    do {
	if (fabs(v->t - t) < T_EPSILON)
	    return;
	if (v->t > t)
	    break;
	v = v->next;
    } while (1);

    // insert a new wvert before v
    if (numwverts >= cWVerts)
	Error("Internal error: didn't allocate enough vertices for tjuncs?");

    newv = pWVerts + numwverts;
    numwverts++;

    newv->t = t;
    newv->next = v;
    newv->prev = v->prev;
    v->prev->next = newv;
    v->prev = newv;
}


/*
===============
AddEdge

===============
*/
static void
AddEdge(vec3_t p1, vec3_t p2)
{
    wedge_t *edge;
    vec_t t1, t2;

    edge = FindEdge(p1, p2, &t1, &t2);
    AddVert(edge, t1);
    AddVert(edge, t2);
}

/*
===============
AddFaceEdges

===============
*/
static void
AddFaceEdges(face_t *f)
{
    int i, j;

    for (i = 0; i < f->w.numpoints; i++) {
	j = (i + 1) % f->w.numpoints;
	AddEdge(f->w.points[i], f->w.points[j]);
    }
}


//============================================================================

// a specially allocated face that can hold hundreds of edges if needed
static byte superfacebuf[8192];
static face_t *superface = (face_t *)superfacebuf;
static face_t *newlist;

static void
SplitFaceForTjunc(face_t *f, face_t *original)
{
    int i;
    face_t *newf, *chain;
    vec3_t dir, test;
    vec_t angle;
    int firstcorner, lastcorner;

    chain = NULL;
    do {
	if (f->w.numpoints <= MAXPOINTS) {
	    /*
	     * the face is now small enough without more cutting so
	     * copy it back to the original
	     */
	    *original = *f;
	    original->original = chain;
	    original->next = newlist;
	    newlist = original;
	    return;
	}

	tjuncfaces++;

      restart:
	/* find the last corner */
	VectorSubtract(f->w.points[f->w.numpoints - 1], f->w.points[0], dir);
	VectorNormalize(dir);
	for (lastcorner = f->w.numpoints - 1; lastcorner > 0; lastcorner--) {
	    VectorSubtract(f->w.points[lastcorner - 1], f->w.points[lastcorner], test);
	    VectorNormalize(test);
	    angle = DotProduct(test, dir);
	    if (angle < 1 - ANGLEEPSILON || angle > 1 + ANGLEEPSILON)
		break;
	}

	/* find the first corner */
	VectorSubtract(f->w.points[1], f->w.points[0], dir);
	VectorNormalize(dir);
	for (firstcorner = 1; firstcorner < f->w.numpoints - 1; firstcorner++) {
	    VectorSubtract(f->w.points[firstcorner + 1], f->w.points[firstcorner],
			   test);
	    VectorNormalize(test);
	    angle = DotProduct(test, dir);
	    if (angle < 1 - ANGLEEPSILON || angle > 1 + ANGLEEPSILON)
		break;
	}

	if (firstcorner + 2 >= MAXPOINTS) {
	    /* rotate the point winding */
	    VectorCopy(f->w.points[0], test);
	    for (i = 1; i < f->w.numpoints; i++)
		VectorCopy(f->w.points[i], f->w.points[i - 1]);
	    VectorCopy(test, f->w.points[f->w.numpoints - 1]);
	    goto restart;
	}

	/*
	 * cut off as big a piece as possible, less than MAXPOINTS, and not
	 * past lastcorner
	 */
	newf = NewFaceFromFace(f);
	if (f->original)
	    Error("original face still exists (%s)", __func__);

	newf->original = chain;
	chain = newf;
	newf->next = newlist;
	newlist = newf;
	if (f->w.numpoints - firstcorner <= MAXPOINTS)
	    newf->w.numpoints = firstcorner + 2;
	else if (lastcorner + 2 < MAXPOINTS &&
		 f->w.numpoints - lastcorner <= MAXPOINTS)
	    newf->w.numpoints = lastcorner + 2;
	else
	    newf->w.numpoints = MAXPOINTS;

	for (i = 0; i < newf->w.numpoints; i++)
	    VectorCopy(f->w.points[i], newf->w.points[i]);
	for (i = newf->w.numpoints - 1; i < f->w.numpoints; i++)
	    VectorCopy(f->w.points[i], f->w.points[i - (newf->w.numpoints - 2)]);

	f->w.numpoints -= (newf->w.numpoints - 2);
    } while (1);
}


/*
===============
FixFaceEdges

===============
*/
static void
FixFaceEdges(face_t *f)
{
    int i, j, k;
    wedge_t *edge;
    wvert_t *v;
    vec_t t1, t2;

    *superface = *f;

 restart:
    for (i = 0; i < superface->w.numpoints; i++) {
	j = (i + 1) % superface->w.numpoints;

	edge = FindEdge(superface->w.points[i], superface->w.points[j], &t1, &t2);

	v = edge->head.next;
	while (v->t < t1 + T_EPSILON)
	    v = v->next;

	if (v->t < t2 - T_EPSILON) {
	    tjuncs++;
	    /* insert a new vertex here */
	    for (k = superface->w.numpoints; k > j; k--)
		VectorCopy(superface->w.points[k - 1], superface->w.points[k]);
	    VectorMA(edge->origin, v->t, edge->dir, superface->w.points[j]);
	    superface->w.numpoints++;
	    goto restart;
	}
    }

    if (superface->w.numpoints <= MAXPOINTS) {
	*f = *superface;
	f->next = newlist;
	newlist = f;
	return;
    }

    /* Too many edges - needs to be split into multiple faces */
    SplitFaceForTjunc(superface, f);
}


//============================================================================

static void
tjunc_count_r(node_t *node)
{
    face_t *f;

    if (node->planenum == PLANENUM_LEAF)
	return;

    for (f = node->faces; f; f = f->next)
	cWVerts += f->w.numpoints;

    tjunc_count_r(node->children[0]);
    tjunc_count_r(node->children[1]);
}

static void
tjunc_find_r(node_t *node)
{
    face_t *f;

    if (node->planenum == PLANENUM_LEAF)
	return;

    for (f = node->faces; f; f = f->next)
	AddFaceEdges(f);

    tjunc_find_r(node->children[0]);
    tjunc_find_r(node->children[1]);
}

static void
tjunc_fix_r(node_t *node)
{
    face_t *f, *next;

    if (node->planenum == PLANENUM_LEAF)
	return;

    newlist = NULL;

    for (f = node->faces; f; f = next) {
	next = f->next;
	FixFaceEdges(f);
    }

    node->faces = newlist;

    tjunc_fix_r(node->children[0]);
    tjunc_fix_r(node->children[1]);
}

/*
===========
tjunc
===========
*/
void
TJunc(const mapentity_t *entity, node_t *headnode)
{
    vec3_t maxs, mins;
    int i;

    Message(msgProgress, "Tjunc");

    /*
     * Guess edges = 1/2 verts
     * Verts are arbitrarily multiplied by 2 because there appears to
     * be a need for them to "grow" slightly.
     */
    cWVerts = 0;
    tjunc_count_r(headnode);
    cWEdges = cWVerts;
    cWVerts *= 2;

    pWVerts = AllocMem(WVERT, cWVerts, true);
    pWEdges = AllocMem(WEDGE, cWEdges, true);

    /*
     * identify all points on common edges
     * origin points won't allways be inside the map, so extend the hash area
     */
    for (i = 0; i < 3; i++) {
	if (fabs(entity->maxs[i]) > fabs(entity->mins[i]))
	    maxs[i] = fabs(entity->maxs[i]);
	else
	    maxs[i] = fabs(entity->mins[i]);
    }
    VectorSubtract(vec3_origin, maxs, mins);

    InitHash(mins, maxs);

    numwedges = numwverts = 0;

    tjunc_find_r(headnode);

    Message(msgStat, "%8d world edges", numwedges);
    Message(msgStat, "%8d edge points", numwverts);

    /* add extra vertexes on edges where needed */
    tjuncs = tjuncfaces = 0;
    tjunc_fix_r(headnode);

    FreeMem(pWVerts, WVERT, cWVerts);
    FreeMem(pWEdges, WEDGE, cWEdges);

    Message(msgStat, "%8d edges added by tjunctions", tjuncs);
    Message(msgStat, "%8d faces added by tjunctions", tjuncfaces);
}
