/*
 *  PathOutline.cpp
 *  nlivarot
 *
 *  Created by fred on Fri Nov 28 2003.
 *
 */

#include "Path.h"
//#include "MyMath.h"
#include <math.h>

void
Path::Outline (Path * dest, double width, JoinType join, ButtType butt,
               double miter)
{
	if ( descr_flags & descr_adding_bezier ) {
		CancelBezier();
	}
	if ( descr_flags & descr_doing_subpath ) {
		CloseSubpath();
	}
	if (descr_nb <= 2) return;
	if (dest == NULL) return;
	dest->Reset ();
	dest->SetBackData (false);
  
	outline_callbacks calls;
	NR::Point endButt, endPos;
	calls.cubicto = StdCubicTo;
	calls.bezierto = StdBezierTo;
	calls.arcto = StdArcTo;
  
	path_descr *sav_descr = descr_cmd;
	int sav_descr_nb = descr_nb;
  
	Path *rev = new Path;
  
	int curP = 0;
	do {
		int lastM = curP;
		do {
			curP++;
			if (curP >= sav_descr_nb) break;
			int typ = sav_descr[curP].flags & descr_type_mask;
			if (typ == descr_moveto) break;
		} while (curP < sav_descr_nb);
		if (curP >= sav_descr_nb) curP = sav_descr_nb;
		if (curP > lastM + 1) {
			// sinon il n'y a qu'un point
			int curD = curP - 1;
			NR::Point curX;
			NR::Point nextX;
			bool needClose = false;
			int firstTyp = sav_descr[curD].flags & descr_type_mask;
			if (firstTyp == descr_close) needClose = true;
			while (curD > lastM && (sav_descr[curD].flags & descr_type_mask) == descr_close) curD--;
			int realP = curD + 1;
			if (curD > lastM) {
				descr_cmd = sav_descr;
				descr_nb = sav_descr_nb;
				curX = PrevPoint (curD);
				rev->Reset ();
				rev->MoveTo (curX);
				while (curD > lastM) {
					int typ = sav_descr[curD].flags & descr_type_mask;
					if (typ == descr_moveto) {
						//                                              rev->Close();
						curD--;
					} else if (typ == descr_forced) {
						//                                              rev->Close();
						curD--;
					} else if (typ == descr_lineto) {
						nextX = PrevPoint (curD - 1);
						rev->LineTo (nextX);
						curX = nextX;
						curD--;
					} else if (typ == descr_cubicto) {
						path_descr_cubicto* nData=(path_descr_cubicto*)(descr_data+sav_descr[curD].dStart);
						nextX = PrevPoint (curD - 1);
						NR::Point  isD=-nData->stD;
						NR::Point  ieD=-nData->enD;
						rev->CubicTo (nextX, ieD,isD);
						curX = nextX;
						curD--;
					} else if (typ == descr_arcto) {
						path_descr_arcto* nData=(path_descr_arcto*)(descr_data+sav_descr[curD].dStart);
						nextX = PrevPoint (curD - 1);
						rev->ArcTo (nextX, nData->rx,nData->ry,nData->angle,nData->large,nData->clockwise);
						curX = nextX;
						curD--;
					} else if (typ == descr_bezierto) {
						nextX = PrevPoint (curD - 1);
						rev->LineTo (nextX);
						curX = nextX;
						curD--;
					}  else if (typ == descr_interm_bezier) {
						int nD = curD - 1;
						while (nD > lastM && (sav_descr[nD].flags & descr_type_mask) != descr_bezierto) nD--;
						if ((sav_descr[nD].flags & descr_type_mask) !=  descr_bezierto)  {
							// pas trouve le debut!?
							// Not find the start?!
							nextX = PrevPoint (nD);
							rev->LineTo (nextX);
							curX = nextX;
						} else {
							nextX = PrevPoint (nD - 1);
							rev->BezierTo (nextX);
							for (int i = curD; i > nD; i--) {
								path_descr_intermbezierto* nData=(path_descr_intermbezierto*)(descr_data+sav_descr[i].dStart);
								rev->IntermBezierTo (nData->p);
							}
							rev->EndBezierTo ();
							curX = nextX;
						}
						curD = nD - 1;
					} else {
						curD--;
					}
				}
				if (needClose) {
					rev->Close ();
					rev->SubContractOutline (dest, calls, 0.0025 * width * width, width,
								 join, butt, miter, true, false, endPos, endButt);
					descr_cmd = sav_descr + lastM;
					descr_nb = realP + 1 - lastM;
					SubContractOutline (dest, calls, 0.0025 * width * width,
							    width, join, butt, miter, true, false, endPos, endButt);
				} else {
					rev->SubContractOutline (dest, calls,  0.0025 * width * width, width,
								 join, butt, miter, false, false, endPos, endButt);
					NR::Point endNor=endButt.ccw();
					if (butt == butt_round) {
						dest->ArcTo (endPos+width*endNor,  1.0001 * width, 1.0001 * width, 0.0, true, true);
					}  else if (butt == butt_square) {
						dest->LineTo (endPos-width*endNor+width*endButt);
						dest->LineTo (endPos+width*endNor+width*endButt);
						dest->LineTo (endPos+width*endNor);
					}  else if (butt == butt_pointy) {
						dest->LineTo (endPos+width*endButt);
						dest->LineTo (endPos+width*endNor);
					} else {
						dest->LineTo (endPos+width*endNor);
					}
					descr_cmd = sav_descr + lastM;
					descr_nb = realP - lastM;
					SubContractOutline (dest, calls, 0.0025 * width * width,  width, join, butt, miter, false, true, endPos, endButt);
					endNor=endButt.ccw();
					if (butt == butt_round) {
						dest->ArcTo (endPos+width*endNor, 1.0001 * width, 1.0001 * width, 0.0, true, true);
					} else if (butt == butt_square) {
						dest->LineTo (endPos-width*endNor+width*endButt);
						dest->LineTo (endPos+width*endNor+width*endButt);
						dest->LineTo (endPos+width*endNor);
					} else if (butt == butt_pointy) {
						dest->LineTo (endPos+width*endButt);
						dest->LineTo (endPos+width*endNor);
					} else {
						dest->LineTo (endPos+width*endNor);
					}
					dest->Close ();
				}
			}
		}
	}
	while (curP < sav_descr_nb);

	delete rev;
	descr_cmd = sav_descr;
	descr_nb = sav_descr_nb;
}

void
Path::OutsideOutline (Path * dest, double width, JoinType join, ButtType butt,
                      double miter)
{
	if (descr_flags & descr_adding_bezier) {
		CancelBezier();
	}
	if (descr_flags & descr_doing_subpath) {
		CloseSubpath();
	}
	if (descr_nb <= 2) return;
	if (dest == NULL) return;
	dest->Reset ();
	dest->SetBackData (false);
  
	outline_callbacks calls;
	NR::Point endButt, endPos;
	calls.cubicto = StdCubicTo;
	calls.bezierto = StdBezierTo;
	calls.arcto = StdArcTo;
	SubContractOutline (dest, calls, 0.0025 * width * width, width, join, butt,
			    miter, true, false, endPos, endButt);
}

void
Path::InsideOutline (Path * dest, double width, JoinType join, ButtType butt,
                     double miter)
{
	if ( descr_flags & descr_adding_bezier ) {
		CancelBezier();
	}
	if ( descr_flags & descr_doing_subpath ) {
		CloseSubpath();
	}
	if (descr_nb <= 2) return;
	if (dest == NULL) return;
	dest->Reset ();
	dest->SetBackData (false);

	outline_callbacks calls;
	NR::Point endButt, endPos;
	calls.cubicto = StdCubicTo;
	calls.bezierto = StdBezierTo;
	calls.arcto = StdArcTo;

	path_descr *sav_descr = descr_cmd;
	int sav_descr_nb = descr_nb;

	Path *rev = new Path;

	int curP = 0;
	do {
		int lastM = curP;
		do {
			curP++;
			if (curP >= sav_descr_nb) break;
			int typ = sav_descr[curP].flags & descr_type_mask;
			if (typ == descr_moveto) break;
		} while (curP < sav_descr_nb);
		if (curP >= sav_descr_nb)  curP = sav_descr_nb;
		if (curP > lastM + 1) {
			// Otherwise there's only one point.  (tr: or "only a point")
			// [sinon il n'y a qu'un point]
			int curD = curP - 1;
			NR::Point curX;
			NR::Point nextX;
			while (curD > lastM && (sav_descr[curD].flags & descr_type_mask) == descr_close) curD--;
			if (curD > lastM) {
				descr_cmd = sav_descr;
				descr_nb = sav_descr_nb;
				curX = PrevPoint (curD);
				rev->Reset ();
				rev->MoveTo (curX);
				while (curD > lastM) {
					int typ = sav_descr[curD].flags & descr_type_mask;
					if (typ == descr_moveto) {
						rev->Close ();
						curD--;
					} else if (typ == descr_forced) {
						curD--;
					} else if (typ == descr_lineto) {
						nextX = PrevPoint (curD - 1);
						rev->LineTo (nextX);
						curX = nextX;
						curD--;
					}  else if (typ == descr_cubicto) {
						path_descr_cubicto* nData=(path_descr_cubicto*)(descr_data+sav_descr[curD].dStart);
						nextX = PrevPoint (curD - 1);
						NR::Point  isD=-nData->stD;
						NR::Point  ieD=-nData->enD;
						rev->CubicTo (nextX, ieD,isD);
						curX = nextX;
						curD--;
					} else if (typ == descr_arcto) {
						path_descr_arcto* nData=(path_descr_arcto*)(descr_data+sav_descr[curD].dStart);
						nextX = PrevPoint (curD - 1);
						rev->ArcTo (nextX, nData->rx,nData->ry,nData->angle,nData->large,nData->clockwise);
						curX = nextX;
						curD--;
					} else if (typ == descr_bezierto) {
						nextX = PrevPoint (curD - 1);
						rev->LineTo (nextX);
						curX = nextX;
						curD--;
					} else if (typ == descr_interm_bezier) {
						int nD = curD - 1;
						while (nD > lastM && (sav_descr[nD].flags & descr_type_mask) != descr_bezierto) nD--;
						if (sav_descr[nD].flags & descr_type_mask != descr_bezierto) {
							// pas trouve le debut!?
							nextX = PrevPoint (nD);
							rev->LineTo (nextX);
							curX = nextX;
						} else {
							nextX = PrevPoint (nD - 1);
							rev->BezierTo (nextX);
							for (int i = curD; i > nD; i--) {
								path_descr_intermbezierto* nData=(path_descr_intermbezierto*)(descr_data+sav_descr[i].dStart);
								rev->IntermBezierTo (nData->p);
							}
							rev->EndBezierTo ();
							curX = nextX;
						}
						curD = nD - 1;
					} else {
						curD--;
					}
				}
				rev->Close ();
				rev->SubContractOutline (dest, calls, 0.0025 * width * width,
							 width, join, butt, miter, true, false,
							 endPos, endButt);
			}
		}
	}  while (curP < sav_descr_nb);
  
	delete rev;
	descr_cmd = sav_descr;
	descr_nb = sav_descr_nb;
}

void
Path::DoOutsideOutline (Path * dest, double width, JoinType join, ButtType butt, double miter, int &stNo, int &enNo)
{
}
void
Path::DoInsideOutline (Path * dest, double width, JoinType join, ButtType butt, double miter, int &stNo, int &enNo)
{
}
void
Path::SubContractOutline (Path * dest, outline_callbacks & calls,
                          double tolerance, double width, JoinType join,
                          ButtType butt, double miter, bool closeIfNeeded,
                          bool skipMoveto, NR::Point & lastP, NR::Point & lastT)
{
	outline_callback_data callsData;
  
	callsData.orig = this;
	callsData.dest = dest;
	int curP = 1;
  
	// le moveto
	NR::Point curX;
	{
		int firstTyp=descr_cmd->flags&descr_type_mask;
		if ( firstTyp != descr_moveto ) {
			curX[0]=curX[1]=0;
			curP=0;
		} else {
			path_descr_moveto* nData=(path_descr_moveto*)(descr_data+descr_cmd->dStart);
			curX = nData->p;
		}
	}
	NR::Point curT(0, 0);
  
	bool doFirst = true;
	NR::Point firstP(0, 0);
	NR::Point firstT(0, 0);
  
	// et le reste, 1 par 1
	while (curP < descr_nb)
	{
		path_descr *curD = descr_cmd + curP;
		int nType = curD->flags & descr_type_mask;
		NR::Point nextX;
		NR::Point stPos, enPos, stTgt, enTgt, stNor, enNor;
		double stRad, enRad, stTle, enTle;
		if (nType == descr_forced)  {
			curP++;
		} else if (nType == descr_moveto) {
			path_descr_moveto* nData=(path_descr_moveto*)(descr_data+curD->dStart);
			nextX = nData->p;
			// et on avance
			if (doFirst) {
			} else {
				if (closeIfNeeded) {
					if ( NR::LInfty (curX- firstP) < 0.0001 ) {
						OutlineJoin (dest, firstP, curT, firstT, width, join,
									 miter);
						dest->Close ();
					}  else {
						path_descr_lineto temp;
						temp.p = firstP;
            
						TangentOnSegAt (0.0, curX, temp, stPos, stTgt,
										stTle);
						TangentOnSegAt (1.0, curX, temp, enPos, enTgt,
										enTle);
						stNor=stTgt.cw();
						enNor=enTgt.cw();
            
						// jointure
						{
							NR::Point pos;
							pos = curX;
							OutlineJoin (dest, pos, curT, stNor, width, join,
										 miter);
						}
						dest->LineTo (enPos+width*enNor);
            
						// jointure
						{
							NR::Point pos;
							pos = firstP;
							OutlineJoin (dest, enPos, enNor, firstT, width, join,
										 miter);
							dest->Close ();
						}
					}
				}
			}
			firstP = nextX;
			curP++;
		}
		else if (nType == descr_close)
		{
			if (doFirst == false)
			{
				if (NR::LInfty (curX - firstP) < 0.0001)
				{
					OutlineJoin (dest, firstP, curT, firstT, width, join,
								 miter);
					dest->Close ();
				}
				else
				{
					path_descr_lineto temp;
					temp.p = firstP;
					nextX = firstP;
          
					TangentOnSegAt (0.0, curX, temp, stPos, stTgt, stTle);
					TangentOnSegAt (1.0, curX, temp, enPos, enTgt, enTle);
					stNor=stTgt.cw();
					enNor=enTgt.cw();
          
					// jointure
					{
						OutlineJoin (dest, stPos, curT, stNor, width, join,
									 miter);
					}
          
					dest->LineTo (enPos+width*enNor);
          
					// jointure
					{
						OutlineJoin (dest, enPos, enNor, firstT, width, join,
									 miter);
						dest->Close ();
					}
				}
			}
			doFirst = true;
			curP++;
		}
		else if (nType == descr_lineto)
		{
			path_descr_lineto* nData=(path_descr_lineto*)(descr_data+curD->dStart);
			nextX = nData->p;
			// test de nullité du segment
			if (IsNulCurve (curD, curX,descr_data))
			{
				curP++;
				continue;
			}
			// et on avance
			TangentOnSegAt (0.0, curX, *nData, stPos, stTgt, stTle);
			TangentOnSegAt (1.0, curX, *nData, enPos, enTgt, enTle);
			stNor=stTgt.cw();
			enNor=enTgt.cw();
      
			lastP = enPos;
			lastT = enTgt;
      
			if (doFirst)
			{
				doFirst = false;
				firstP = stPos;
				firstT = stNor;
				if (skipMoveto)
				{
					skipMoveto = false;
				}
				else
					dest->MoveTo (curX+width*stNor);
			}
			else
			{
				// jointure
				NR::Point pos;
				pos = curX;
				OutlineJoin (dest, pos, curT, stNor, width, join, miter);
			}
      
			int n_d = dest->LineTo (nextX+width*enNor);
			if (n_d >= 0)
			{
				dest->descr_cmd[n_d].associated = curP;
				dest->descr_cmd[n_d].tSt = 0.0;
				dest->descr_cmd[n_d].tEn = 1.0;
			}
			curP++;
		}
		else if (nType == descr_cubicto)
		{
			path_descr_cubicto* nData=(path_descr_cubicto*)(descr_data+curD->dStart);
			nextX = nData->p;
			// test de nullité du segment
			if (IsNulCurve (curD, curX,descr_data))
			{
				curP++;
				continue;
			}
			// et on avance
			TangentOnCubAt (0.0, curX, *nData, false, stPos, stTgt,
							stTle, stRad);
			TangentOnCubAt (1.0, curX, *nData, true, enPos, enTgt,
							enTle, enRad);
			stNor=stTgt.cw();
			enNor=enTgt.cw();
      
			lastP = enPos;
			lastT = enTgt;
      
			if (doFirst)
			{
				doFirst = false;
				firstP = stPos;
				firstT = stNor;
				if (skipMoveto)
				{
					skipMoveto = false;
				}
				else
					dest->MoveTo (curX+width*stNor);
			}
			else
			{
				// jointure
				NR::Point pos;
				pos = curX;
				OutlineJoin (dest, pos, curT, stNor, width, join, miter);
			}
      
			callsData.piece = curP;
			callsData.tSt = 0.0;
			callsData.tEn = 1.0;
			callsData.x1 = curX[0];
			callsData.y1 = curX[1];
			callsData.x2 = nextX[0];
			callsData.y2 = nextX[1];
			callsData.d.c.dx1 = nData->stD[0];
			callsData.d.c.dy1 = nData->stD[1];
			callsData.d.c.dx2 = nData->enD[0];
			callsData.d.c.dy2 = nData->enD[1];
			(calls.cubicto) (&callsData, tolerance, width);
      
			curP++;
		}
		else if (nType == descr_arcto)
		{
			path_descr_arcto* nData=(path_descr_arcto*)(descr_data+curD->dStart);
			nextX = nData->p;
			// test de nullité du segment
			if (IsNulCurve (curD, curX,descr_data))
			{
				curP++;
				continue;
			}
			// et on avance
			TangentOnArcAt (0.0, curX, *nData, stPos, stTgt, stTle,
							stRad);
			TangentOnArcAt (1.0, curX, *nData, enPos, enTgt, enTle,
							enRad);
			stNor=stTgt.cw();
			enNor=enTgt.cw();
      
			lastP = enPos;
			lastT = enTgt;	// tjs definie
      
			if (doFirst)
			{
				doFirst = false;
				firstP = stPos;
				firstT = stNor;
				if (skipMoveto)
				{
					skipMoveto = false;
				}
				else
					dest->MoveTo (curX+width*stNor);
			}
			else
			{
				// jointure
				NR::Point pos;
				pos = curX;
				OutlineJoin (dest, pos, curT, stNor, width, join, miter);
			}
      
			callsData.piece = curP;
			callsData.tSt = 0.0;
			callsData.tEn = 1.0;
			callsData.x1 = curX[0];
			callsData.y1 = curX[1];
			callsData.x2 = nextX[0];
			callsData.y2 = nextX[1];
			callsData.d.a.rx = nData->rx;
			callsData.d.a.ry = nData->ry;
			callsData.d.a.angle = nData->angle;
			callsData.d.a.clock = nData->clockwise;
			callsData.d.a.large = nData->large;
			(calls.arcto) (&callsData, tolerance, width);
      
			curP++;
		}
		else if (nType == descr_bezierto)
		{
			path_descr_bezierto* nBData=(path_descr_bezierto*)(descr_data+curD->dStart);
			int nbInterm = nBData->nb;
			nextX = nBData->p;
      
			if (IsNulCurve (curD, curX,descr_data)) {
				curP += nbInterm + 1;
				continue;
			}
      
//      path_descr *bezStart = curD;
			curP++;
			curD = descr_cmd + curP;
			path_descr *intermPoints = curD;
			path_descr_intermbezierto* nData=(path_descr_intermbezierto*)(descr_data+intermPoints->dStart);
     
			if (nbInterm <= 0) {
				// et on avance
				path_descr_lineto temp;
				temp.p = nextX;
				TangentOnSegAt (0.0, curX, temp, stPos, stTgt, stTle);
				TangentOnSegAt (1.0, curX, temp, enPos, enTgt, enTle);
				stNor=stTgt.cw();
				enNor=enTgt.cw();
        
				lastP = enPos;
				lastT = enTgt;
        
				if (doFirst) {
					doFirst = false;
					firstP = stPos;
					firstT = stNor;
					if (skipMoveto) {
						skipMoveto = false;
					} else dest->MoveTo (curX+width*stNor);
				} else {
					// jointure
					NR::Point pos;
					pos = curX;
					if (stTle > 0) OutlineJoin (dest, pos, curT, stNor, width, join, miter);
				}
				int n_d = dest->LineTo (nextX+width*enNor);
				if (n_d >= 0) {
					dest->descr_cmd[n_d].associated = curP - 1;
					dest->descr_cmd[n_d].tSt = 0.0;
					dest->descr_cmd[n_d].tEn = 1.0;
				}
			} else if (nbInterm == 1) {
				NR::Point  midX;
				midX = nData->p;
				// et on avance
				TangentOnBezAt (0.0, curX, *nData, *nBData, false, stPos, stTgt, stTle, stRad);
				TangentOnBezAt (1.0, curX, *nData, *nBData, true, enPos, enTgt, enTle, enRad);
				stNor=stTgt.cw();
				enNor=enTgt.cw();
        
				lastP = enPos;
				lastT = enTgt;
        
				if (doFirst) {
					doFirst = false;
					firstP = stPos;
					firstT = stNor;
					if (skipMoveto) {
						skipMoveto = false;
					} else dest->MoveTo (curX+width*stNor);
				}  else {
					// jointure
					NR::Point pos;
					pos = curX;
					OutlineJoin (dest, pos, curT, stNor, width, join, miter);
				}
        
				callsData.piece = curP;
				callsData.tSt = 0.0;
				callsData.tEn = 1.0;
				callsData.x1 = curX[0];
				callsData.y1 = curX[1];
				callsData.x2 = nextX[0];
				callsData.y2 = nextX[1];
				callsData.d.b.mx = midX[0];
				callsData.d.b.my = midX[1];
				(calls.bezierto) (&callsData, tolerance, width);
        
			} else if (nbInterm > 1) {
				NR::Point  bx=curX;
				NR::Point cx=curX;
				NR::Point dx=curX;
        
				dx = nData->p;
				TangentOnBezAt (0.0, curX, *nData, *nBData, false, stPos, stTgt, stTle, stRad);
				stNor=stTgt.cw();
        
				intermPoints++;
				nData=(path_descr_intermbezierto*)(descr_data+intermPoints->dStart);       
				// et on avance
				if (stTle > 0) {
					if (doFirst) {
						doFirst = false;
						firstP = stPos;
						firstT = stNor;
						if (skipMoveto) {
							skipMoveto = false;
						} else  dest->MoveTo (curX+width*stNor);
					} else {
						// jointure
						NR::Point pos=curX;
						OutlineJoin (dest, pos, stTgt, stNor, width, join,  miter);
						//                                              dest->LineTo(curX+width*stNor.x,curY+width*stNor.y);
					}
				}
        
				cx = 2 * bx - dx;
        
				for (int k = 0; k < nbInterm - 1; k++) {
					bx = cx;
					cx = dx;
          
					dx = nData->p;
					intermPoints++;
					nData=(path_descr_intermbezierto*)(descr_data+intermPoints->dStart);         
					NR::Point stx = (bx + cx) / 2;
					//                                      double  stw=(bw+cw)/2;
          
					path_descr_bezierto tempb;
					path_descr_intermbezierto tempi;
					tempb.nb = 1;
					tempb.p = (cx + dx) / 2;
					tempi.p = cx;
					TangentOnBezAt (1.0, stx, tempi, tempb, true, enPos, enTgt, enTle, enRad);
					enNor=enTgt.cw();
          
					lastP = enPos;
					lastT = enTgt;
          
					callsData.piece = curP + k;
					callsData.tSt = 0.0;
					callsData.tEn = 1.0;
					callsData.x1 = stx[0];
					callsData.y1 = stx[1];
					callsData.x2 = (cx[0] + dx[0]) / 2;
					callsData.y2 = (cx[1] + dx[1]) / 2;
					callsData.d.b.mx = cx[0];
					callsData.d.b.my = cx[1];
					(calls.bezierto) (&callsData, tolerance, width);
				}
				{
					bx = cx;
					cx = dx;
          
					dx = nextX;
					dx = 2 * dx - cx;
          
					NR::Point stx = (bx + cx) / 2;
					//                                      double  stw=(bw+cw)/2;
          
					path_descr_bezierto tempb;
					path_descr_intermbezierto tempi;
					tempb.nb = 1;
					tempb.p = (cx + dx) / 2;
					tempi.p = cx;
					TangentOnBezAt (1.0, stx, tempi, tempb, true, enPos,
									enTgt, enTle, enRad);
					enNor=enTgt.cw();
          
					lastP = enPos;
					lastT = enTgt;
          
					callsData.piece = curP + nbInterm - 1;
					callsData.tSt = 0.0;
					callsData.tEn = 1.0;
					callsData.x1 = stx[0];
					callsData.y1 = stx[1];
					callsData.x2 = (cx[0] + dx[0]) / 2;
					callsData.y2 = (cx[1] + dx[1]) / 2;
					callsData.d.b.mx = cx[0];
					callsData.d.b.my = cx[1];
					(calls.bezierto) (&callsData, tolerance, width);
          
				}
			}
      
			// et on avance
			curP += nbInterm;
		}
		curX = nextX;
		curT = enNor;		// sera tjs bien definie
	}
	if (closeIfNeeded)
	{
		if (doFirst == false)
		{
		}
	}
}

/*
 *
 * utilitaires pour l'outline
 *
 */

bool
Path::IsNulCurve (path_descr const * curD, NR::Point const &curX,NR::Point* ddata)
{
	switch(curD->flags & descr_type_mask) {
    case descr_lineto:
    {
		path_descr_lineto* nData=(path_descr_lineto*)(ddata+curD->dStart);
		if (NR::LInfty(nData->p - curX) < 0.00001) {
			return true;
		}
		return false;
    }
	case descr_cubicto:
    {
		path_descr_cubicto* nData=(path_descr_cubicto*)(ddata+curD->dStart);
		NR::Point A = nData->stD + nData->enD + 2*(curX - nData->p);
		NR::Point B = 3*(nData->p - curX) - 2*nData->stD - nData->enD;
		NR::Point C = nData->stD;
		if (NR::LInfty(A) < 0.0001 
			&& NR::LInfty(B) < 0.0001 
			&& NR::LInfty (C) < 0.0001) {
			return true;
		}
		return false;
    }
    case descr_arcto:
    {
		path_descr_arcto* nData=(path_descr_arcto*)(ddata+curD->dStart);
		if ( NR::LInfty(nData->p - curX) < 0.00001) {
			if ((nData->large == false) 
				|| (fabsf (nData->rx) < 0.00001
					|| fabsf (nData->ry) < 0.00001)) {
				return true;
			}
		}
		return false;
    }
    case descr_bezierto:
    {
		path_descr_bezierto* nBData=(path_descr_bezierto*)(ddata+curD->dStart);
		if (nBData->nb <= 0)
		{
			if (NR::LInfty(nBData->p - curX) < 0.00001) {
				return true;
			}
			return false;
		}
		else if (nBData->nb == 1)
		{
			if (NR::LInfty(nBData->p - curX) < 0.00001) {
				path_descr const *interm = curD + 1;
				path_descr_intermbezierto* nData=(path_descr_intermbezierto*)(ddata+interm->dStart);
				if (NR::LInfty(nData->p - curX) < 0.00001) {
					return true;
				}
			}
			return false;
		} else if (NR::LInfty(nBData->p - curX) < 0.00001) {
			for (int i = 1; i <= nBData->nb; i++) {
				path_descr const *interm = curD + i;
				path_descr_intermbezierto* nData=(path_descr_intermbezierto*)(ddata+interm->dStart);
				if (NR::LInfty(nData->p - curX) > 0.00001) {
					return false;
				}
			}
			return true;
		}
    }
    default:
		return true;
	}
}

void
Path::TangentOnSegAt (double at, NR::Point const &iS, path_descr_lineto & fin,
                      NR::Point & pos, NR::Point & tgt, double &len)
{
	NR::Point iE = fin.p;
	NR::Point seg = iE - iS;
	double l = L2(seg);
	if (l <= 0.000001) {
		pos = iS;
		tgt = NR::Point(0, 0);
		len = 0;
	} else {
		tgt = seg / l;
		pos = (1 - at)*iS + at*iE;
		len = l;
	}
}

void
Path::TangentOnArcAt (double at, const NR::Point &iS, path_descr_arcto & fin,
                      NR::Point & pos, NR::Point & tgt, double &len, double &rad)
{
	NR::Point iE;
	iE = fin.p;
	double rx, ry, angle;
	rx = fin.rx;
	ry = fin.ry;
	angle = fin.angle;
	bool large, wise;
	large = fin.large;
	wise = fin.clockwise;
  
	pos = iS;
	tgt[0] = tgt[1] = 0;
	if (rx <= 0.0001 || ry <= 0.0001)
		return;
  
	double sex = iE[0] - iS[0], sey = iE[1] - iS[1];
	double ca = cos (angle), sa = sin (angle);
	double csex = ca * sex + sa * sey, csey = -sa * sex + ca * sey;
	csex /= rx;
	csey /= ry;
	double l = csex * csex + csey * csey;
	if (l >= 4)
		return;
	const double d = sqrt(std::max(1 - l / 4, 0.0));
	double csdx = csey, csdy = -csex;
	l = sqrt (l);
	csdx /= l;
	csdy /= l;
	csdx *= d;
	csdy *= d;
  
	double sang, eang;
	double rax = -csdx - csex / 2, ray = -csdy - csey / 2;
	if (rax < -1)
	{
		sang = M_PI;
	}
	else if (rax > 1)
	{
		sang = 0;
	}
	else
	{
		sang = acos (rax);
		if (ray < 0)
			sang = 2 * M_PI - sang;
	}
	rax = -csdx + csex / 2;
	ray = -csdy + csey / 2;
	if (rax < -1)
	{
		eang = M_PI;
	}
	else if (rax > 1)
	{
		eang = 0;
	}
	else
	{
		eang = acos (rax);
		if (ray < 0)
			eang = 2 * M_PI - eang;
	}
  
	csdx *= rx;
	csdy *= ry;
	double drx = ca * csdx - sa * csdy, dry = sa * csdx + ca * csdy;
  
	if (wise)
	{
		if (large == true)
		{
			drx = -drx;
			dry = -dry;
			double swap = eang;
			eang = sang;
			sang = swap;
			eang += M_PI;
			sang += M_PI;
			if (eang >= 2 * M_PI)
				eang -= 2 * M_PI;
			if (sang >= 2 * M_PI)
				sang -= 2 * M_PI;
		}
	}
	else
	{
		if (large == false)
		{
			drx = -drx;
			dry = -dry;
			double swap = eang;
			eang = sang;
			sang = swap;
			eang += M_PI;
			sang += M_PI;
			if (eang >= 2 * M_PI)
				eang -= 2 * M_PI;
			if (sang >= 2 * M_PI)
				sang -= 2 * M_PI;
		}
	}
	drx += (iS[0] + iE[0]) / 2;
	dry += (iS[1] + iE[1]) / 2;
  
	if (wise) {
		if (sang < eang)
			sang += 2 * M_PI;
		double b = sang * (1 - at) + eang * at;
		double cb = cos (b), sb = sin (b);
		pos[0] = drx + ca * rx * cb - sa * ry * sb;
		pos[1] = dry + sa * rx * cb + ca * ry * sb;
		tgt[0] = ca * rx * sb + sa * ry * cb;
		tgt[1] = sa * rx * sb - ca * ry * cb;
		NR::Point dtgt;
		dtgt[0] = -ca * rx * cb + sa * ry * sb;
		dtgt[1] = -sa * rx * cb - ca * ry * sb;
		len = L2(tgt);
		rad = len * dot(tgt, tgt) / (tgt[0] * dtgt[1] - tgt[1] * dtgt[0]);
		tgt /= len;
	}
	else
	{
		if (sang > eang)
			sang -= 2 * M_PI;
		double b = sang * (1 - at) + eang * at;
		double cb = cos (b), sb = sin (b);
		pos[0] = drx + ca * rx * cb - sa * ry * sb;
		pos[1] = dry + sa * rx * cb + ca * ry * sb;
		tgt[0] = ca * rx * sb + sa * ry * cb;
		tgt[1] = sa * rx * sb - ca * ry * cb;
		NR::Point dtgt;
		dtgt[0] = -ca * rx * cb + sa * ry * sb;
		dtgt[1] = -sa * rx * cb - ca * ry * sb;
		len = L2(tgt);
		rad = len * dot(tgt, tgt) / (tgt[0] * dtgt[1] - tgt[1] * dtgt[0]);
		tgt /= len;
	}
}
void
Path::TangentOnCubAt (double at, NR::Point const &iS, path_descr_cubicto & fin,
                      bool before, NR::Point & pos, NR::Point & tgt, double &len,
                      double &rad)
{
	const NR::Point E = fin.p;
	const NR::Point Sd = fin.stD;
	const NR::Point Ed = fin.enD;
	
	pos = iS;
	tgt = NR::Point(0,0);
	len = rad = 0;
	
	const NR::Point A = Sd + Ed - 2*E + 2*iS;
	const NR::Point B = 0.5*(Ed - Sd);
	const NR::Point C = 0.25*(6*E - 6*iS - Sd - Ed);
	const NR::Point D = 0.125*(4*iS + 4*E - Ed + Sd);
	const double atb = at - 0.5;
	pos = (atb * atb * atb)*A + (atb * atb)*B + atb*C + D;
	const NR::Point der = (3 * atb * atb)*A  + (2 * atb)*B + C;
	const NR::Point dder = (6 * atb)*A + 2*B;
	const NR::Point ddder = 6 * A;
	
	double l = NR::L2 (der);
	if (l <= 0.0001) {
		len = 0;
		l = L2(dder);
		if (l <= 0.0001) {
			l = L2(ddder);
			if (l <= 0.0001) {
				// pas de segment....
				return;
			}
			rad = 100000000;
			tgt = ddder / l;
			if (before) {
				tgt = -tgt;
			}
			return;
		}
		rad = -l * (dot(dder,dder)) / (cross(ddder,dder));
		tgt = dder / l;
		if (before) {
			tgt = -tgt;
		}
		return;
	}
	len = l;
	
	rad = -l * (dot(der,der)) / (cross(dder,der));
	
	tgt = der / l;
}

void
Path::TangentOnBezAt (double at, NR::Point const &iS,
                      path_descr_intermbezierto & mid,
                      path_descr_bezierto & fin, bool before, NR::Point & pos,
                      NR::Point & tgt, double &len, double &rad)
{
	pos = iS;
	tgt = NR::Point(0,0);
	len = rad = 0;
	
	const NR::Point A = fin.p + iS - 2*mid.p;
	const NR::Point B = 2*mid.p - 2 * iS;
	const NR::Point C = iS;
	
	pos = at * at * A + at * B + C;
	const NR::Point der = 2 * at * A + B;
	const NR::Point dder = 2 * A;
	double l = NR::L2(der);
	
	if (l <= 0.0001) {
		l = NR::L2(dder);
		if (l <= 0.0001) {
			// pas de segment....
			// Not a segment.
			return;
		}
		rad = 100000000; // Why this number?
		tgt = dder / l;
		if (before) {
			tgt = -tgt;
		}
		return;
	}
	len = l;
	rad = -l * (dot(der,der)) / (cross(dder,der));
	
	tgt = der / l;
}

void
Path::OutlineJoin (Path * dest, NR::Point pos, NR::Point stNor, NR::Point enNor, double width,
                   JoinType join, double miter)
{
	const double angSi = cross (enNor,stNor);
	const double angCo = dot (stNor, enNor);
	// 1/1000 is very big/ugly, but otherwise it stuffs things up a little...
	// 1/1000 est tres grossier, mais sinon ca merde tout azimut
	if ((width >= 0 && angSi > -0.001) 
	    || (width < 0 && angSi < 0.001)) {
		if (angCo > 0.999) {
			// straight ahead
			// tout droit
		} else if (angCo < -0.999) {
			// half turn
			// demit-tour
			dest->LineTo (pos + width*enNor);
		} else {
			dest->LineTo (pos);
			dest->LineTo (pos + width*enNor);
		}
	} else {
		if (join == join_round) {
			// Use the ends of the cubic: approximate the arc at the
			// point where .., and support better the rounding of
			// coordinates of the end points.
      
			// utiliser des bouts de cubique: approximation de l'arc (au point ou on en est...), et supporte mieux 
			// l'arrondi des coordonnees des extremites
			/* double   angle=acos(angCo);
			   if ( angCo >= 0 ) {
			   NR::Point   stTgt,enTgt;
			   RotCCWTo(stNor,stTgt);
			   RotCCWTo(enNor,enTgt);
			   dest->CubicTo(pos.x+width*enNor.x,pos.y+width*enNor.y,
			   angle*width*stTgt.x,angle*width*stTgt.y,
			   angle*width*enTgt.x,angle*width*enTgt.y);
			   } else {
			   NR::Point   biNor;
			   NR::Point   stTgt,enTgt,biTgt;
			   biNor.x=stNor.x+enNor.x;
			   biNor.y=stNor.y+enNor.y;
			   double  biL=sqrt(biNor.x*biNor.x+biNor.y*biNor.y);
			   biNor.x/=biL;
			   biNor.y/=biL;
			   RotCCWTo(stNor,stTgt);
			   RotCCWTo(enNor,enTgt);
			   RotCCWTo(biNor,biTgt);
			   dest->CubicTo(pos.x+width*biNor.x,pos.y+width*biNor.y,
			   angle*width*stTgt.x,angle*width*stTgt.y,
			   angle*width*biTgt.x,angle*width*biTgt.y);
			   dest->CubicTo(pos.x+width*enNor.x,pos.y+width*enNor.y,
			   angle*width*biTgt.x,angle*width*biTgt.y,
			   angle*width*enTgt.x,angle*width*enTgt.y);
			   }*/
			if (width > 0) {
				dest->ArcTo (pos + width*enNor,
							 1.0001 * width, 1.0001 * width, 0.0, false, true);
			} else {
				dest->ArcTo (pos + width*enNor,
							 -1.0001 * width, -1.0001 * width, 0.0, false,
							 false);
			}
		} else if (join == join_pointy) {
			NR::Point biss = stNor + enNor;
			const double lb = NR::L2(biss);
			biss /= lb;
			const double angCo = dot (biss, enNor);
			const double l = width / angCo;
			miter = std::max(miter, 0.5 * lb);
			if (l > miter) {
				const double angSi = cross (biss, stNor);
				const double r = (l - miter) * angCo / angSi;
				NR::Point corner = miter*biss + pos;
				dest->LineTo (corner + r*biss.ccw());
				dest->LineTo (corner - r*biss.ccw());
				dest->LineTo (pos + width*enNor);
			} else {
				dest->LineTo (pos+l*biss);
				dest->LineTo (pos+width*enNor);
			}
		} else {
			dest->LineTo (pos + width*enNor);
		}
	}
}

// les callbacks

void
Path::RecStdCubicTo (outline_callback_data * data, double tol, double width,
                     int lev)
{
	NR::Point stPos, miPos, enPos;
	NR::Point stTgt, enTgt, miTgt, stNor, enNor, miNor;
	double stRad, miRad, enRad;
	double stTle, miTle, enTle;
	// un cubic
	{
		path_descr_cubicto temp;
		temp.p = NR::Point(data->x2, data->y2);
		temp.stD = NR::Point(data->d.c.dx1, data->d.c.dy1);
		temp.enD = NR::Point(data->d.c.dx2, data->d.c.dy2);
		NR::Point initial_point(data->x1, data->y1);
		TangentOnCubAt (0.0, initial_point, temp, false, stPos, stTgt, stTle,
						stRad);
		TangentOnCubAt (0.5, initial_point, temp, false, miPos, miTgt, miTle,
						miRad);
		TangentOnCubAt (1.0, initial_point, temp, true, enPos, enTgt, enTle,
						enRad);
		stNor=stTgt.cw();
		miNor=miTgt.cw();
		enNor=enTgt.cw();
	}
  
	double stGue = 1, miGue = 1, enGue = 1;
	if (fabsf (stRad) > 0.01)
		stGue += width / stRad;
	if (fabsf (miRad) > 0.01)
		miGue += width / miRad;
	if (fabsf (enRad) > 0.01)
		enGue += width / enRad;
	stGue *= stTle;
	miGue *= miTle;
	enGue *= enTle;
  
  
	if (lev <= 0) {
		int n_d = data->dest->CubicTo (enPos + width*enNor, 
									   stGue*stTgt,
									   enGue*enTgt);
		if (n_d >= 0) {
			data->dest->descr_cmd[n_d].associated = data->piece;
			data->dest->descr_cmd[n_d].tSt = data->tSt;
			data->dest->descr_cmd[n_d].tEn = data->tEn;
		}
		return;
	}
  
	NR::Point chk;
	const NR::Point req = miPos + width * miNor;
	{
		path_descr_cubicto temp;
		double chTle, chRad;
		NR::Point chTgt;
		temp.p = enPos + width * enNor;
		temp.stD = stGue * stTgt;
		temp.enD = enGue * enTgt;
		TangentOnCubAt (0.5, stPos+width*stNor,
						temp, false, chk, chTgt, chTle, chRad);
	}
	const NR::Point diff = req - chk;
	const double err = dot(diff,diff);
	if (err <= tol * tol) {
		int n_d = data->dest->CubicTo (enPos + width*enNor,
									   stGue*stTgt,
									   enGue*enTgt);
		if (n_d >= 0) {
			data->dest->descr_cmd[n_d].associated = data->piece;
			data->dest->descr_cmd[n_d].tSt = data->tSt;
			data->dest->descr_cmd[n_d].tEn = data->tEn;
		}
	} else {
		outline_callback_data desc = *data;
	  
		desc.tSt = data->tSt;
		desc.tEn = (data->tSt + data->tEn) / 2;
		desc.x1 = data->x1;
		desc.y1 = data->y1;
		desc.x2 = miPos[0];
		desc.y2 = miPos[1];
		desc.d.c.dx1 = 0.5 * stTle * stTgt[0];
		desc.d.c.dy1 = 0.5 * stTle * stTgt[1];
		desc.d.c.dx2 = 0.5 * miTle * miTgt[0];
		desc.d.c.dy2 = 0.5 * miTle * miTgt[1];
		RecStdCubicTo (&desc, tol, width, lev - 1);
	  
		desc.tSt = (data->tSt + data->tEn) / 2;
		desc.tEn = data->tEn;
		desc.x1 = miPos[0];
		desc.y1 = miPos[1];
		desc.x2 = data->x2;
		desc.y2 = data->y2;
		desc.d.c.dx1 = 0.5 * miTle * miTgt[0];
		desc.d.c.dy1 = 0.5 * miTle * miTgt[1];
		desc.d.c.dx2 = 0.5 * enTle * enTgt[0];
		desc.d.c.dy2 = 0.5 * enTle * enTgt[1];
		RecStdCubicTo (&desc, tol, width, lev - 1);
	}
}

void
Path::StdCubicTo (Path::outline_callback_data * data, double tol, double width)
{
	fflush (stdout);
	RecStdCubicTo (data, tol, width, 8);
}

void
Path::StdBezierTo (Path::outline_callback_data * data, double tol, double width)
{
	path_descr_bezierto tempb;
	path_descr_intermbezierto tempi;
	tempb.nb = 1;
	tempb.p = NR::Point(data->x2, data->y2);
	tempi.p = NR::Point(data->d.b.mx, data->d.b.my);
	NR::Point stPos, enPos, stTgt, enTgt;
	double stRad, enRad, stTle, enTle;
	NR::Point  tmp(data->x1,data->y1);
	TangentOnBezAt (0.0, tmp, tempi, tempb, false, stPos, stTgt,
					stTle, stRad);
	TangentOnBezAt (1.0, tmp, tempi, tempb, true, enPos, enTgt,
					enTle, enRad);
	data->d.c.dx1 = stTle * stTgt[0];
	data->d.c.dy1 = stTle * stTgt[1];
	data->d.c.dx2 = enTle * enTgt[0];
	data->d.c.dy2 = enTle * enTgt[1];
	RecStdCubicTo (data, tol, width, 8);
}

void
Path::RecStdArcTo (outline_callback_data * data, double tol, double width,
                   int lev)
{
	NR::Point stPos, miPos, enPos;
	NR::Point stTgt, enTgt, miTgt, stNor, enNor, miNor;
	double stRad, miRad, enRad;
	double stTle, miTle, enTle;
	// un cubic
	{
		path_descr_arcto temp;
		temp.p[0] = data->x2;
		temp.p[1] = data->y2;
		temp.rx = data->d.a.rx;
		temp.ry = data->d.a.ry;
		temp.angle = data->d.a.angle;
		temp.clockwise = data->d.a.clock;
		temp.large = data->d.a.large;
		NR::Point tmp(data->x1,data->y1);
		TangentOnArcAt (data->d.a.stA, tmp, temp, stPos, stTgt,
						stTle, stRad);
		TangentOnArcAt ((data->d.a.stA + data->d.a.enA) / 2, tmp,
						temp, miPos, miTgt, miTle, miRad);
		TangentOnArcAt (data->d.a.enA, tmp, temp, enPos, enTgt,
						enTle, enRad);
		stNor=stTgt.cw();
		miNor=miTgt.cw();
		enNor=enTgt.cw();
	}
  
	double stGue = 1, miGue = 1, enGue = 1;
	if (fabsf (stRad) > 0.01)
		stGue += width / stRad;
	if (fabsf (miRad) > 0.01)
		miGue += width / miRad;
	if (fabsf (enRad) > 0.01)
		enGue += width / enRad;
	stGue *= stTle;
	miGue *= miTle;
	enGue *= enTle;
	double sang, eang;
	{
		NR::Point  tms(data->x1,data->y1),tme(data->x2,data->y2);
		ArcAngles (tms,tme, data->d.a.rx,
				   data->d.a.ry, data->d.a.angle, data->d.a.large, !data->d.a.clock,
				   sang, eang);
	}
	double scal = eang - sang;
	if (scal < 0)
		scal += 2 * M_PI;
	if (scal > 2 * M_PI)
		scal -= 2 * M_PI;
	scal *= data->d.a.enA - data->d.a.stA;
  
	if (lev <= 0)
	{
		int n_d = data->dest->CubicTo (enPos + width*enNor,
									   stGue*scal*stTgt,
									   enGue*scal*enTgt);
		if (n_d >= 0) {
			data->dest->descr_cmd[n_d].associated = data->piece;
			data->dest->descr_cmd[n_d].tSt = data->d.a.stA;
			data->dest->descr_cmd[n_d].tEn = data->d.a.enA;
		}
		return;
	}
  
	NR::Point chk;
	const NR::Point req = miPos + width*miNor;
	{
		path_descr_cubicto temp;
		double chTle, chRad;
		NR::Point chTgt;
		temp.p = enPos + width * enNor;
		temp.stD = stGue * scal * stTgt;
		temp.enD = enGue * scal * enTgt;
		TangentOnCubAt (0.5, stPos+width*stNor,
						temp, false, chk, chTgt, chTle, chRad);
	}
	const NR::Point diff = req - chk;
	const double err = (dot(diff,diff));
	if (err <= tol * tol)
	{
		int n_d = data->dest->CubicTo (enPos + width*enNor,
									   stGue*scal*stTgt,
									   enGue*scal*enTgt);
		if (n_d >= 0) {
			data->dest->descr_cmd[n_d].associated = data->piece;
			data->dest->descr_cmd[n_d].tSt = data->d.a.stA;
			data->dest->descr_cmd[n_d].tEn = data->d.a.enA;
		}
	} else {
		outline_callback_data desc = *data;
	  
		desc.d.a.stA = data->d.a.stA;
		desc.d.a.enA = (data->d.a.stA + data->d.a.enA) / 2;
		RecStdArcTo (&desc, tol, width, lev - 1);
	  
		desc.d.a.stA = (data->d.a.stA + data->d.a.enA) / 2;
		desc.d.a.enA = data->d.a.enA;
		RecStdArcTo (&desc, tol, width, lev - 1);
	}
}

void
Path::StdArcTo (Path::outline_callback_data * data, double tol, double width)
{
	data->d.a.stA = 0.0;
	data->d.a.enA = 1.0;
	RecStdArcTo (data, tol, width, 8);
}
