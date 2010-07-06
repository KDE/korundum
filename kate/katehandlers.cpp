/***************************************************************************
                 plasmahandlers.cpp  -  Plasma specific marshallers
                             -------------------
    begin                : Sun Sep 28 2003
    copyright            : (C) 2003 by Richard Dale
    email                : Richard_Dale@tipitina.demon.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <ruby.h>

#include <qtruby.h>
#include <smokeruby.h>
#include <marshall_macros.h>

#include <kate/mainwindow.h>

DEF_LIST_MARSHALLER( KateMainWindowList, QList<Kate::MainWindow*>, Kate::MainWindow )

TypeHandler Kate_handlers[] = {
    { "QList<Kate::MainWindow*>", marshall_KateMainWindowList },
    { "QList<Kate::MainWindow*>&", marshall_KateMainWindowList },
    { 0, 0 }
};
