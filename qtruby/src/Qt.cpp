/***************************************************************************
                          Qt.cpp  -  description
                             -------------------
    begin                : Fri Jul 4 2003
    copyright            : (C) 2003-2006 by Richard Dale
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdarg.h>

#include <QtCore/qabstractitemmodel.h>			
#include <QtCore/qglobal.h>
#include <QtCore/qhash.h>
#include <QtCore/qline.h>			
#include <QtCore/qmetaobject.h>
#include <QtCore/qobject.h>
#include <QtCore/qrect.h>			
#include <QtCore/qregexp.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>
#include <QtGui/qapplication.h>
#include <QtGui/qbitmap.h>			
#include <QtGui/qcolor.h>			
#include <QtGui/qcursor.h>
#include <QtGui/qfont.h>			
#include <QtGui/qicon.h>			
#include <QtGui/qitemselectionmodel.h>
#include <QtGui/qpalette.h>			
#include <QtGui/qpen.h>			
#include <QtGui/qpixmap.h>			
#include <QtGui/qpolygon.h>			
#include <QtGui/qtextformat.h>			
#include <QtGui/qwidget.h>

#ifdef QT_QTDBUS
#include <QtDBus/qdbusargument.h>
#endif

#undef DEBUG
#ifndef __USE_POSIX
#define __USE_POSIX
#endif
#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif
#ifdef _BOOL
#define HAS_BOOL
#endif

#include <ruby.h>

#include <smoke/smoke.h>
#include <smoke/qt_smoke.h>

#undef free
#undef malloc

#include "marshall.h"
#include "qtruby.h"
#include "smokeruby.h"
#include "smoke.h"
#include "marshall_types.h"
// #define DEBUG

extern "C" {
VALUE qt_internal_module = Qnil;
VALUE qt_module = Qnil;

VALUE qlistmodel_class = Qnil;
VALUE qmetaobject_class = Qnil;
VALUE qtablemodel_class = Qnil;
VALUE qt_base_class = Qnil;
VALUE qvariant_class = Qnil;

VALUE moduleindex_class = Qnil;

bool application_terminated = false;
}

QList<Smoke*> smokeList;
QHash<Smoke*, QtRubyModule> qtruby_modules;

#ifdef DEBUG
int do_debug = qtdb_gc;
#else
int do_debug = qtdb_none;
#endif

typedef QHash<void *, VALUE *> PointerMap;
Q_GLOBAL_STATIC(PointerMap, pointer_map)
int object_count = 0;

// FIXME:
// Don't the two following hashs create memory leaks by using pointers to Smoke::(Module)Index ?
QHash<QByteArray, Smoke::ModuleIndex *> methcache;
QHash<QByteArray, Smoke::ModuleIndex *> classcache;

// FIXME:
// find some way to use a Smoke::ModuleIndex here instead of an int
// Maps from an int id to classname in the form Qt::Widget
QHash<Smoke::ModuleIndex, QByteArray*> classname;

#define logger logger_backend

Smoke::ModuleIndex _current_method = { 0, 0 };

smokeruby_object * 
alloc_smokeruby_object(bool allocated, Smoke * smoke, int classId, void * ptr)
{
    smokeruby_object * o = ALLOC(smokeruby_object);
	o->classId = classId;
	o->smoke = smoke;
	o->ptr = ptr;
	o->allocated = allocated;
	return o;
}

void
free_smokeruby_object(smokeruby_object * o)
{
	xfree(o);
	return;
}

smokeruby_object *value_obj_info(VALUE ruby_value) {  // ptr on success, null on fail
	if (TYPE(ruby_value) != T_DATA) {
		return 0;
	}

    smokeruby_object * o = 0;
    Data_Get_Struct(ruby_value, smokeruby_object, o);
    return o;
}

void *value_to_ptr(VALUE ruby_value) {  // ptr on success, null on fail
    smokeruby_object *o = value_obj_info(ruby_value);
    return o;
}

VALUE getPointerObject(void *ptr) {
	if (!pointer_map() || !pointer_map()->contains(ptr)) {
		if (do_debug & qtdb_gc) {
			qWarning("getPointerObject %p -> nil", ptr);
			if (!pointer_map()) {
				qWarning("getPointerObject pointer_map deleted");
			}
		}
	    return Qnil;
	} else {
		if (do_debug & qtdb_gc) {
			qWarning("getPointerObject %p -> %p", ptr, (void *) *(pointer_map()->operator[](ptr)));
		}
		return *(pointer_map()->operator[](ptr));
	}
}

void unmapPointer(smokeruby_object *o, Smoke::Index classId, void *lastptr) {
	void *ptr = o->smoke->cast(o->ptr, o->classId, classId);
	if (ptr != lastptr) {
		lastptr = ptr;
		if (pointer_map() && pointer_map()->contains(ptr)) {
			VALUE * obj_ptr = pointer_map()->operator[](ptr);
		
			if (do_debug & qtdb_gc) {
				const char *className = o->smoke->classes[o->classId].className;
				qWarning("unmapPointer (%s*)%p -> %p size: %d", className, ptr, obj_ptr, pointer_map()->size() - 1);
			}
	    
			pointer_map()->remove(ptr);
			xfree((void*) obj_ptr);
		}
    }

	for (Smoke::Index *i = o->smoke->inheritanceList + o->smoke->classes[classId].parents; *i; i++) {
		unmapPointer(o, *i, lastptr);
	}
}

// Store pointer in pointer_map hash : "pointer_to_Qt_object" => weak ref to associated Ruby object
// Recurse to store it also as casted to its parent classes.

void mapPointer(VALUE obj, smokeruby_object *o, Smoke::Index classId, void *lastptr) {
    void *ptr = o->smoke->cast(o->ptr, o->classId, classId);
	
    if (ptr != lastptr) {
		lastptr = ptr;
		VALUE * obj_ptr = ALLOC(VALUE);
		memcpy(obj_ptr, &obj, sizeof(VALUE));
		
		if (do_debug & qtdb_gc) {
			const char *className = o->smoke->classes[o->classId].className;
			qWarning("mapPointer (%s*)%p -> %p size: %d", className, ptr, (void*)obj, pointer_map()->size() + 1);
		}
	
		pointer_map()->insert(ptr, obj_ptr);
    }
	
	for (Smoke::Index *i = o->smoke->inheritanceList + o->smoke->classes[classId].parents; *i; i++) {
		mapPointer(obj, o, *i, lastptr);
	}
	
	return;
}


QtRubySmokeBinding::QtRubySmokeBinding(Smoke *s) : SmokeBinding(s) {}

void
QtRubySmokeBinding::deleted(Smoke::Index classId, void *ptr) {
	if (!pointer_map()) {
	return;
	}
	VALUE obj = getPointerObject(ptr);
	smokeruby_object *o = value_obj_info(obj);
	if (do_debug & qtdb_gc) {
    	qWarning("%p->~%s()", ptr, smoke->className(classId));
    }
	if (!o || !o->ptr) {
    	return;
	}
	unmapPointer(o, o->classId, 0);
	o->ptr = 0;
}

bool
QtRubySmokeBinding::callMethod(Smoke::Index method, void *ptr, Smoke::Stack args, bool /*isAbstract*/) {
	VALUE obj = getPointerObject(ptr);
	smokeruby_object *o = value_obj_info(obj);

	if (do_debug & qtdb_virtual) {
		Smoke::Method & meth = smoke->methods[method];
		QByteArray signature(smoke->methodNames[meth.name]);
		signature += "(";
			for (int i = 0; i < meth.numArgs; i++) {
		if (i != 0) signature += ", ";
			signature += smoke->types[smoke->argumentList[meth.args + i]].name;
		}
		signature += ")";
		if (meth.flags & Smoke::mf_const) {
			signature += " const";
		}
			qWarning(	"virtual %p->%s::%s called", 
				ptr,
						smoke->classes[smoke->methods[method].classId].className,
					(const char *) signature );
	}

	if (o == 0) {
    	if( do_debug & qtdb_virtual )   // if not in global destruction
			qWarning("Cannot find object for virtual method %p -> %p", ptr, &obj);
    	return false;
	}
	const char *methodName = smoke->methodNames[smoke->methods[method].name];
	if (qstrncmp(methodName, "operator", sizeof("operator") - 1) == 0) {
		methodName += (sizeof("operator") - 1);
	}
		// If the virtual method hasn't been overriden, just call the C++ one.
	if (rb_respond_to(obj, rb_intern(methodName)) == 0) {
    	return false;
	}
	VirtualMethodCall c(smoke, method, args, obj, ALLOCA_N(VALUE, smoke->methods[method].numArgs));
	c.next();
	return true;
}

char*
QtRubySmokeBinding::className(Smoke::Index classId) {
	Smoke::ModuleIndex mi = { smoke, classId };
	return (char *) (const char *) *(classname.value(mi));
}

/*
	Converts a C++ value returned by a signal invocation to a Ruby 
	reply type
*/
class SignalReturnValue : public Marshall {
    QList<MocArgument*>	_replyType;
    Smoke::Stack _stack;
	VALUE * _result;
public:
	SignalReturnValue(void ** o, VALUE * result, QList<MocArgument*> replyType) 
	{
		_result = result;
		_replyType = replyType;
		_stack = new Smoke::StackItem[1];
		smokeStackFromQtStack(_stack, o, 0, 1, _replyType);
		Marshall::HandlerFn fn = getMarshallFn(type());
		(*fn)(this);
    }

    SmokeType type() { 
		return _replyType[0]->st; 
	}
    Marshall::Action action() { return Marshall::ToVALUE; }
    Smoke::StackItem &item() { return _stack[0]; }
    VALUE * var() {
    	return _result;
    }
	
	void unsupported() 
	{
		rb_raise(rb_eArgError, "Cannot handle '%s' as signal reply-type", type().name());
    }
	Smoke *smoke() { return type().smoke(); }
    
	void next() {}
    
	bool cleanup() { return false; }
	
	~SignalReturnValue() {
		delete[] _stack;
	}
};

/* Note that the SignalReturnValue and EmitSignal classes really belong in
	marshall_types.cpp. However, for unknown reasons they don't link with certain
	versions of gcc. So they were moved here in to work round that bug.
*/
EmitSignal::EmitSignal(QObject *obj, int id, int /*items*/, QList<MocArgument*> args, VALUE *sp, VALUE * result) : SigSlotBase(args),
    _obj(obj), _id(id)
{ 
	_sp = sp;
	_result = result;
}

Marshall::Action 
EmitSignal::action() 
{ 
	return Marshall::FromVALUE; 
}

Smoke::StackItem &
EmitSignal::item() 
{ 
	return _stack[_cur]; 
}

const char *
EmitSignal::mytype() 
{ 
	return "signal"; 
}

void 
EmitSignal::emitSignal() 
{
	if (_called) return;
	_called = true;
	void ** o = new void*[_items];
	smokeStackToQtStack(_stack, o + 1, 1, _items, _args);
	void * ptr;
	o[0] = &ptr;
	prepareReturnValue(o);

	_obj->metaObject()->activate(_obj, _id, o);

	if (_args[0]->argType != xmoc_void) {
		SignalReturnValue r(o, _result, _args);
	}
	delete[] o;
}

void 
EmitSignal::mainfunction() 
{ 
	emitSignal(); 
}

bool 
EmitSignal::cleanup() 
{ 
	return true; 
}

InvokeNativeSlot::InvokeNativeSlot(QObject *obj, int id, int /*items*/, QList<MocArgument*> args, VALUE *sp, VALUE * result) : SigSlotBase(args),
    _obj(obj), _id(id)
{ 
	_sp = sp;
	_result = result;
}

Marshall::Action 
InvokeNativeSlot::action() 
{ 
	return Marshall::FromVALUE; 
}

Smoke::StackItem &
InvokeNativeSlot::item() 
{ 
	return _stack[_cur]; 
}

const char *
InvokeNativeSlot::mytype() 
{ 
	return "slot"; 
}

void 
InvokeNativeSlot::invokeSlot() 
{
	if (_called) return;
	_called = true;
	void ** o = new void*[_items];
	smokeStackToQtStack(_stack, o + 1, 1, _items, _args);
	void * ptr;
	o[0] = &ptr;
	prepareReturnValue(o);

	_obj->qt_metacall(QMetaObject::InvokeMetaMethod, _id, o);
	
	if (_args[0]->argType != xmoc_void) {
		SignalReturnValue r(o, _result, _args);
	}
	delete[] o;
}

void 
InvokeNativeSlot::mainfunction() 
{ 
	invokeSlot(); 
}

bool 
InvokeNativeSlot::cleanup() 
{ 
	return true; 
}

void rb_str_catf(VALUE self, const char *format, ...) 
{
#define CAT_BUFFER_SIZE 2048
static char p[CAT_BUFFER_SIZE];
	va_list ap;
	va_start(ap, format);
    qvsnprintf(p, CAT_BUFFER_SIZE, format, ap);
	p[CAT_BUFFER_SIZE - 1] = '\0';
	rb_str_cat2(self, p);
	va_end(ap);
}

const char *
resolve_classname(smokeruby_object * o)
{
    if (o->smoke->classes[o->classId].external) {
        Smoke::ModuleIndex mi = o->smoke->findClass(o->smoke->className(o->classId));
        o->smoke = mi.smoke;
        o->classId = mi.index;
        return qtruby_modules.value(mi.smoke).resolve_classname(o);
    }
    return qtruby_modules.value(o->smoke).resolve_classname(o);
}

VALUE
findMethod(VALUE /*self*/, VALUE c_value, VALUE name_value)
{
    char *c = StringValuePtr(c_value);
    char *name = StringValuePtr(name_value);
    VALUE result = rb_ary_new();
    Smoke* s = Smoke::classMap[c];
    Smoke::ModuleIndex meth = qt_Smoke->NullModuleIndex;
    if (s) meth = s->findMethod(c, name);
#ifdef DEBUG
    if (do_debug & qtdb_calls) qWarning("Found method %s::%s => %d", c, name, meth.index);
#endif
    if(!meth.index) {
        // since every smoke module defines a class 'QGlobalSpace' we can't rely on the classMap,
        // so we search for methods by hand
        foreach (Smoke* s, smokeList) {
            Smoke::ModuleIndex cid = s->idClass("QGlobalSpace");
            Smoke::ModuleIndex mnid = s->idMethodName(name);
            if (!cid.index || !mnid.index) continue;
            meth = s->idMethod(cid.index, mnid.index);
            if (meth.index) break;
        }
#ifdef DEBUG
        if (do_debug & qtdb_calls) qWarning("Found method QGlobalSpace::%s => %d", name, meth.index);
#endif
    }

    if (!meth.index) {
        return result;
    // empty list
    } else if(meth.index > 0) {
        Smoke::Index i = meth.smoke->methodMaps[meth.index].method;
        if (i == 0) {		// shouldn't happen
            rb_raise(rb_eArgError, "Corrupt method %s::%s", c, name);
        } else if(i > 0) {	// single match
            Smoke::Method &methodRef = meth.smoke->methods[i];
            if ((methodRef.flags & Smoke::mf_internal) == 0) {
                rb_ary_push(result, rb_funcall(moduleindex_class, rb_intern("new"), 2, INT2NUM(smokeList.indexOf(meth.smoke)), INT2NUM(i)));
            }
        } else {		// multiple match
            i = -i;		// turn into ambiguousMethodList index
            while (meth.smoke->ambiguousMethodList[i]) {
                Smoke::Method &methodRef = meth.smoke->methods[meth.smoke->ambiguousMethodList[i]];
                if ((methodRef.flags & Smoke::mf_internal) == 0) {
                    rb_ary_push(result, rb_funcall(moduleindex_class, rb_intern("new"), 2, INT2NUM(smokeList.indexOf(meth.smoke)), INT2NUM(meth.smoke->ambiguousMethodList[i])));
//#ifdef DEBUG
                    if (do_debug & qtdb_calls) qWarning("Ambiguous Method %s::%s => %d", c, name, meth.smoke->ambiguousMethodList[i]);
//#endif

                }
            i++;
            }
	    }
    }
    return result;
}

// findAllMethods(ModuleIndex [, startingWith]) : returns { "mungedName" => [index in methods, ...], ... }

VALUE
findAllMethods(int argc, VALUE * argv, VALUE /*self*/)
{
    VALUE rb_mi = argv[0];
    VALUE result = rb_hash_new();
    if (rb_mi != Qnil) {
        Smoke::Index c = (Smoke::Index) NUM2INT(rb_funcall(rb_mi, rb_intern("index"), 0));
        Smoke *smoke = smokeList[NUM2INT(rb_funcall(rb_mi, rb_intern("smoke"), 0))];
        if (c > smoke->numClasses) {
            return Qnil;
        }
        char * pat = 0L;
        if(argc > 1 && TYPE(argv[1]) == T_STRING)
            pat = StringValuePtr(argv[1]);
#ifdef DEBUG
        if (do_debug & qtdb_calls) qWarning("findAllMethods called with classid = %d, pat == %s", c, pat);
#endif
        Smoke::Index imax = smoke->numMethodMaps;
        Smoke::Index imin = 0, icur = -1, methmin, methmax;
        methmin = -1; methmax = -1; // kill warnings
        int icmp = -1;
        while(imax >= imin) {
            icur = (imin + imax) / 2;
            icmp = smoke->leg(smoke->methodMaps[icur].classId, c);
            if (icmp == 0) {
                Smoke::Index pos = icur;
                while (icur && smoke->methodMaps[icur-1].classId == c)
                    icur --;
                methmin = icur;
                icur = pos;
                while(icur < imax && smoke->methodMaps[icur+1].classId == c)
                    icur ++;
                methmax = icur;
                break;
            }
            if (icmp > 0)
                imax = icur - 1;
            else
                imin = icur + 1;
        }
        if (icmp == 0) {
            for (Smoke::Index i = methmin; i <= methmax; i++) {
                Smoke::Index m = smoke->methodMaps[i].name;
                if (pat == 0L || strncmp(smoke->methodNames[m], pat, strlen(pat)) == 0) {
                    Smoke::Index ix = smoke->methodMaps[i].method;
                    VALUE meths = rb_ary_new();
                    if (ix >= 0) {	// single match
                        Smoke::Method &methodRef = smoke->methods[ix];
                        if ((methodRef.flags & Smoke::mf_internal) == 0) {
                            rb_ary_push(meths, rb_funcall(moduleindex_class, rb_intern("new"), 2, INT2NUM(smokeList.indexOf(smoke)), INT2NUM((int) ix)));
                        }
                    } else {		// multiple match
                        ix = -ix;		// turn into ambiguousMethodList index
                        while (smoke->ambiguousMethodList[ix]) {
                            Smoke::Method &methodRef = smoke->methods[smoke->ambiguousMethodList[ix]];
                            if ((methodRef.flags & Smoke::mf_internal) == 0) {
                                rb_ary_push(meths, rb_funcall(moduleindex_class, rb_intern("new"), 2, INT2NUM(smokeList.indexOf(smoke)), INT2NUM((int)smoke->ambiguousMethodList[ix])));
                            }
                            ix++;
                        }
                    }
                    rb_hash_aset(result, rb_str_new2(smoke->methodNames[m]), meths);
                }
            }
        }
    }
    return result;
}

/*
	Flags values
		0					All methods, except enum values and protected non-static methods
		mf_static			Static methods only
		mf_enum				Enums only
		mf_protected		Protected non-static methods only
*/

#define PUSH_QTRUBY_METHOD		\
		if (	(methodRef.flags & (Smoke::mf_internal|Smoke::mf_ctor|Smoke::mf_dtor)) == 0 \
				&& strcmp(qt_Smoke->methodNames[methodRef.name], "operator=") != 0 \
				&& strcmp(qt_Smoke->methodNames[methodRef.name], "operator!=") != 0 \
				&& strcmp(qt_Smoke->methodNames[methodRef.name], "operator--") != 0 \
				&& strcmp(qt_Smoke->methodNames[methodRef.name], "operator++") != 0 \
				&& strncmp(qt_Smoke->methodNames[methodRef.name], "operator ", strlen("operator ")) != 0 \
				&& (	(flags == 0 && (methodRef.flags & (Smoke::mf_static|Smoke::mf_enum|Smoke::mf_protected)) == 0) \
						|| (	flags == Smoke::mf_static \
								&& (methodRef.flags & Smoke::mf_enum) == 0 \
								&& (methodRef.flags & Smoke::mf_static) == Smoke::mf_static ) \
						|| (flags == Smoke::mf_enum && (methodRef.flags & Smoke::mf_enum) == Smoke::mf_enum) \
						|| (	flags == Smoke::mf_protected \
								&& (methodRef.flags & Smoke::mf_static) == 0 \
								&& (methodRef.flags & Smoke::mf_protected) == Smoke::mf_protected ) ) ) { \
			if (strncmp(qt_Smoke->methodNames[methodRef.name], "operator", strlen("operator")) == 0) { \
				if (op_re.indexIn(qt_Smoke->methodNames[methodRef.name]) != -1) { \
					rb_ary_push(result, rb_str_new2((op_re.cap(1) + op_re.cap(2)).toLatin1())); \
				} else { \
					rb_ary_push(result, rb_str_new2(qt_Smoke->methodNames[methodRef.name] + strlen("operator"))); \
				} \
			} else if (predicate_re.indexIn(qt_Smoke->methodNames[methodRef.name]) != -1 && methodRef.numArgs == 0) { \
				rb_ary_push(result, rb_str_new2((predicate_re.cap(2).toLower() + predicate_re.cap(3) + "?").toLatin1())); \
			} else if (set_re.indexIn(qt_Smoke->methodNames[methodRef.name]) != -1 && methodRef.numArgs == 1) { \
				rb_ary_push(result, rb_str_new2((set_re.cap(2).toLower() + set_re.cap(3) + "=").toLatin1())); \
			} else { \
				rb_ary_push(result, rb_str_new2(qt_Smoke->methodNames[methodRef.name])); \
			} \
		}
 
VALUE
findAllMethodNames(VALUE /*self*/, VALUE result, VALUE classid, VALUE flags_value)
{
	QRegExp predicate_re("^(is|has)(.)(.*)");
	QRegExp set_re("^(set)([A-Z])(.*)");
	QRegExp op_re("operator(.*)(([-%~/+|&*])|(>>)|(<<)|(&&)|(\\|\\|)|(\\*\\*))=$");

	unsigned short flags = (unsigned short) NUM2UINT(flags_value);
	if (classid != Qnil) {
		Smoke::Index c = (Smoke::Index) NUM2INT(rb_funcall(classid, rb_intern("index"), 0));
		Smoke* s = smokeList[NUM2INT(rb_funcall(classid, rb_intern("smoke"), 0))];
		if (c > s->numClasses) {
			return Qnil;
		}
#ifdef DEBUG
		if (do_debug & qtdb_calls) qWarning("findAllMethodNames called with classid = %d in module %s", c, s->moduleName());
#endif
		Smoke::Index imax = s->numMethodMaps;
		Smoke::Index imin = 0, icur = -1, methmin, methmax;
		methmin = -1; methmax = -1; // kill warnings
		int icmp = -1;

		while (imax >= imin) {
			icur = (imin + imax) / 2;
			icmp = s->leg(s->methodMaps[icur].classId, c);
			if (icmp == 0) {
				Smoke::Index pos = icur;
				while(icur && s->methodMaps[icur-1].classId == c)
					icur --;
				methmin = icur;
				icur = pos;
				while(icur < imax && s->methodMaps[icur+1].classId == c)
					icur ++;
				methmax = icur;
				break;
			}
			if (icmp > 0)
				imax = icur - 1;
			else
				imin = icur + 1;
		}

        if (icmp == 0) {
 			for (Smoke::Index i=methmin ; i <= methmax ; i++) {
				Smoke::Index ix= s->methodMaps[i].method;
				if (ix >= 0) {	// single match
					Smoke::Method &methodRef = s->methods[ix];
					PUSH_QTRUBY_METHOD
				} else {		// multiple match
					ix = -ix;		// turn into ambiguousMethodList index
					while (s->ambiguousMethodList[ix]) {
						Smoke::Method &methodRef = s->methods[s->ambiguousMethodList[ix]];
						PUSH_QTRUBY_METHOD
						ix++;
					}
				}
            }
        }
    }
    return result;
}

QByteArray *
find_cached_selector(int argc, VALUE * argv, VALUE klass, const char * methodName)
{
    // Look in the cache
static QByteArray * mcid = 0;
	if (mcid == 0) {
		mcid = new QByteArray();
	}

	*mcid = rb_class2name(klass);
	*mcid += ';';
	*mcid += methodName;
	for(int i=4; i<argc ; i++)
	{
		*mcid += ';';
		*mcid += get_VALUEtype(argv[i]);
	}
	Smoke::ModuleIndex *rcid = methcache.value(*mcid);
#ifdef DEBUG
	if (do_debug & qtdb_calls) qWarning("method_missing mcid: %s", (const char *) *mcid);
#endif
	
	if (rcid) {
		// Got a hit
#ifdef DEBUG
		if (do_debug & qtdb_calls) qWarning("method_missing cache hit, mcid: %s", (const char *) *mcid);
#endif
		_current_method.smoke = rcid->smoke;
		_current_method.index = rcid->index;
	} else {
		_current_method.smoke = 0;
		_current_method.index = -1;
	}
	
	return mcid;
}

VALUE
method_missing(int argc, VALUE * argv, VALUE self)
{
	const char * methodName = rb_id2name(SYM2ID(argv[0]));
    VALUE klass = rb_funcall(self, rb_intern("class"), 0);

    VALUE retval = Qnil;

	// Look for 'thing?' methods, and try to match isThing() or hasThing() in the Smoke runtime
static QByteArray * pred = 0;
	if (pred == 0) {
		pred = new QByteArray();
	}
	
	*pred = methodName;
	if (pred->endsWith("?")) {
		smokeruby_object *o = value_obj_info(self);
		if(!o || !o->ptr) {
			return rb_call_super(argc, argv);
		}
		
		// Drop the trailing '?'
		pred->replace(pred->length() - 1, 1, "");
		
		pred->replace(0, 1, pred->mid(0, 1).toUpper());
		pred->replace(0, 0, "is");
		Smoke::ModuleIndex meth = o->smoke->findMethod(o->smoke->classes[o->classId].className, (const char *) *pred);
		
		if (meth.index == 0) {
			pred->replace(0, 2, "has");
			meth = o->smoke->findMethod(o->smoke->classes[o->classId].className, *pred);
		}
		
		if (meth.index > 0) {
			methodName = (char *) (const char *) *pred;
		}
	}
		
	VALUE * temp_stack = ALLOCA_N(VALUE, argc+3);
    temp_stack[0] = rb_str_new2("Qt");
    temp_stack[1] = rb_str_new2(methodName);
    temp_stack[2] = klass;
    temp_stack[3] = self;
    for (int count = 1; count < argc; count++) {
		temp_stack[count+3] = argv[count];
    }

	{
		QByteArray * mcid = find_cached_selector(argc+3, temp_stack, klass, methodName);

		if (_current_method.index == -1) {
			// Find the C++ method to call. Do that from Ruby for now

			retval = rb_funcall2(qt_internal_module, rb_intern("do_method_missing"), argc+3, temp_stack);
			if (_current_method.index == -1) {
				const char * op = rb_id2name(SYM2ID(argv[0]));
				if (	qstrcmp(op, "-") == 0
						|| qstrcmp(op, "+") == 0
						|| qstrcmp(op, "/") == 0
						|| qstrcmp(op, "%") == 0
						|| qstrcmp(op, "|") == 0 )
				{
					// Look for operator methods of the form 'operator+=', 'operator-=' and so on..
					char op1[3];
					op1[0] = op[0];
					op1[1] = '=';
					op1[2] = '\0';
					temp_stack[1] = rb_str_new2(op1);
					retval = rb_funcall2(qt_internal_module, rb_intern("do_method_missing"), argc+3, temp_stack);
				}

				if (_current_method.index == -1) {
					// Check for property getter/setter calls, and for slots in QObject classes
					// not in the smoke library
					smokeruby_object *o = value_obj_info(self);
					if (	o != 0 
							&& o->ptr != 0 
							&& o->smoke->isDerivedFrom(o->smoke, o->classId, o->smoke->idClass("QObject").smoke, o->smoke->idClass("QObject").index) )
					{
						QObject * qobject = (QObject *) o->smoke->cast(o->ptr, o->classId, o->smoke->idClass("QObject").index);
static QByteArray * name = 0;
						if (name == 0) {
							name = new QByteArray();
						}
						
						*name = rb_id2name(SYM2ID(argv[0]));
						const QMetaObject * meta = qobject->metaObject();

						if (argc == 1) {
							if (name->endsWith("?")) {
								name->replace(0, 1, pred->mid(0, 1).toUpper());
								name->replace(0, 0, "is");
								if (meta->indexOfProperty(*name) == -1) {
									name->replace(0, 2, "has");
								}
							}

							if (meta->indexOfProperty(*name) != -1) {
								VALUE qvariant = rb_funcall(self, rb_intern("property"), 1, rb_str_new2(*name));
								return rb_funcall(qvariant, rb_intern("value"), 0);
							}
						}

						if (argc == 2 && name->endsWith("=")) {
							name->replace("=", "");
							if (meta->indexOfProperty(*name) != -1) {
								VALUE qvariant = rb_funcall(self, rb_intern("qVariantFromValue"), 1, argv[1]);
								return rb_funcall(self, rb_intern("setProperty"), 2, rb_str_new2(*name), qvariant);
							}
						}

						int classId = o->smoke->idClass(meta->className()).index;

						// The class isn't in the Smoke lib. But if it is called 'local::Merged'
						// it is from a QDBusInterface and the slots are remote, so don't try to
						// those.
						while (	classId == 0 
								&& qstrcmp(meta->className(), "local::Merged") != 0
								&& qstrcmp(meta->superClass()->className(), "QDBusAbstractInterface") != 0 ) 
						{
							// Assume the QObject has slots which aren't in the Smoke library, so try
							// and call the slot directly
							for (int id = meta->methodOffset(); id < meta->methodCount(); id++) {
								if (meta->method(id).methodType() == QMetaMethod::Slot) {
									QByteArray signature(meta->method(id).signature());
									QByteArray methodName = signature.mid(0, signature.indexOf('('));

									// Don't check that the types of the ruby args match the c++ ones for now,
									// only that the name and arg count is the same.
									if (*name == methodName && meta->method(id).parameterTypes().count() == (argc - 1)) {
										QList<MocArgument*> args = get_moc_arguments(	o->smoke, meta->method(id).typeName(), 
																						meta->method(id).parameterTypes() );
										VALUE result = Qnil;
										InvokeNativeSlot slot(qobject, id, argc - 1, args, argv + 1, &result);
										slot.next();
										return result;
									}
								}
							}
							meta = meta->superClass();
							classId = o->smoke->idClass(meta->className()).index;
						}
					}
					
					return rb_call_super(argc, argv);
				}
			}
			// Success. Cache result.
			methcache.insert(*mcid, new Smoke::ModuleIndex(_current_method));
		}
	}
    MethodCall c(_current_method.smoke, _current_method.index, self, temp_stack+4, argc-1);
    c.next();
    VALUE result = *(c.var());
    return result;
}

VALUE
class_method_missing(int argc, VALUE * argv, VALUE klass)
{
	VALUE result = Qnil;
	VALUE retval = Qnil;
	const char * methodName = rb_id2name(SYM2ID(argv[0]));
	VALUE * temp_stack = ALLOCA_N(VALUE, argc+3);
    temp_stack[0] = rb_str_new2("Qt");
    temp_stack[1] = rb_str_new2(methodName);
    temp_stack[2] = klass;
    temp_stack[3] = Qnil;
    for (int count = 1; count < argc; count++) {
		temp_stack[count+3] = argv[count];
    }

    {
		QByteArray * mcid = find_cached_selector(argc+3, temp_stack, klass, methodName);

		if (_current_method.index == -1) {
			retval = rb_funcall2(qt_internal_module, rb_intern("do_method_missing"), argc+3, temp_stack);
			if (_current_method.index != -1) {
				// Success. Cache result.
				methcache.insert(*mcid, new Smoke::ModuleIndex(_current_method));
			}
		}
	}

    if (_current_method.index == -1) {
static QRegExp * rx = 0;
		if (rx == 0) {
			rx = new QRegExp("[a-zA-Z]+");
		}
		
		if (rx->indexIn(methodName) == -1) {
			// If an operator method hasn't been found as an instance method,
			// then look for a class method - after 'op(self,a)' try 'self.op(a)' 
	    	VALUE * method_stack = ALLOCA_N(VALUE, argc - 1);
	    	method_stack[0] = argv[0];
	    	for (int count = 1; count < argc - 1; count++) {
				method_stack[count] = argv[count+1];
    		}
			result = method_missing(argc-1, method_stack, argv[1]);
			return result;
		} else {
			return rb_call_super(argc, argv);
		}
    }
    MethodCall c(_current_method.smoke, _current_method.index, Qnil, temp_stack+4, argc-1);
    c.next();
    result = *(c.var());
    return result;
}

QList<MocArgument*>
get_moc_arguments(Smoke* smoke, const char * typeName, QList<QByteArray> methodTypes)
{
static QRegExp * rx = 0;
	if (rx == 0) {
		rx = new QRegExp("^(bool|int|uint|long|ulong|double|char\\*|QString)&?$");
	}

	methodTypes.prepend(QByteArray(typeName));
	QList<MocArgument*> result;

	foreach (QByteArray name, methodTypes) {
		MocArgument *arg = new MocArgument;
		Smoke::Index typeId = 0;

		if (name.isEmpty()) {
			arg->argType = xmoc_void;
			result.append(arg);
		} else {
			name.replace("const ", "");
			QString staticType = (rx->indexIn(name) != -1 ? rx->cap(1) : "ptr");
			if (staticType == "ptr") {
				arg->argType = xmoc_ptr;
				typeId = smoke->idType(name.constData());
				if (typeId == 0 && !name.contains('*')) {
					name += "&";
					typeId = smoke->idType(name.constData());
				}
			} else if (staticType == "bool") {
				arg->argType = xmoc_bool;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "int") {
				arg->argType = xmoc_int;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "uint") {
				arg->argType = xmoc_uint;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "long") {
				arg->argType = xmoc_long;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "ulong") {
				arg->argType = xmoc_ulong;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "double") {
				arg->argType = xmoc_double;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "char*") {
				arg->argType = xmoc_charstar;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "QString") {
				arg->argType = xmoc_QString;
				name += "*";
				typeId = smoke->idType(name.constData());
			}

			if (typeId == 0) {
				rb_raise(rb_eArgError, "Cannot handle '%s' as slot argument\n", name.constData());
				return result;
			}

			arg->st.set(smoke, typeId);
			result.append(arg);
		}
	}

	return result;
}

extern "C"
{
// ----------------   Helpers -------------------

//---------- All functions except fully qualified statics & enums ---------

VALUE
mapObject(VALUE self, VALUE obj)
{
    smokeruby_object *o = value_obj_info(obj);
    if(!o)
        return Qnil;
    mapPointer(obj, o, o->classId, 0);
    return self;
}

VALUE set_obj_info(const char * className, smokeruby_object * o);

VALUE
qobject_metaobject(VALUE self)
{
	smokeruby_object * o = value_obj_info(self);
	QObject * qobject = (QObject *) o->smoke->cast(o->ptr, o->classId, o->smoke->idClass("QObject").index);
	QMetaObject * meta = (QMetaObject *) qobject->metaObject();
	VALUE obj = getPointerObject(meta);
	if (obj != Qnil) {
		return obj;
	}

	smokeruby_object  * m = alloc_smokeruby_object(	false, 
													o->smoke, 
													o->smoke->idClass("QMetaObject").index, 
													meta );

	obj = set_obj_info("Qt::MetaObject", m);
	return obj;
}

VALUE
set_obj_info(const char * className, smokeruby_object * o)
{
    VALUE klass = rb_funcall(qt_internal_module,
			     rb_intern("find_class"),
			     1,
			     rb_str_new2(className) );
	if (klass == Qnil) {
		rb_raise(rb_eRuntimeError, "Class '%s' not found", className);
	}

	Smoke::ModuleIndex *r = classcache.value(className);
	if (r != 0) {
		o->classId = (int) r->index;
	}
	// If the instance is a subclass of QObject, then check to see if the
	// className from its QMetaObject is in the Smoke library. If not then
	// create a Ruby class for it dynamically. Remove the first letter from 
	// any class names beginning with 'Q' or 'K' and put them under the Qt:: 
	// or KDE:: modules respectively.
	if (o->smoke->isDerivedFrom(o->smoke, o->classId, o->smoke->idClass("QObject").smoke, o->smoke->idClass("QObject").index)) {
		QObject * qobject = (QObject *) o->smoke->cast(o->ptr, o->classId, o->smoke->idClass("QObject").index);
		const QMetaObject * meta = qobject->metaObject();
		int classId = o->smoke->idClass(meta->className()).index;
		// The class isn't in the Smoke lib..
		if (classId == 0) {
			VALUE new_klass = Qnil;
			QByteArray className(meta->className());

			if (className == "QTableModel") {
				new_klass = qtablemodel_class;
			} else if (className == "QListModel") {
				new_klass = qlistmodel_class;
			} else if (className.startsWith("Q")) {
				className.replace("Q", "");
				className = className.mid(0, 1).toUpper() + className.mid(1);
    				new_klass = rb_define_class_under(qt_module, className, klass);
			} else {
    				new_klass = rb_define_class(className, klass);
			}

			if (new_klass != Qnil) {
				klass = new_klass;

				for (int id = meta->enumeratorOffset(); id < meta->enumeratorCount(); id++) {
					// If there are any enum keys with the same scope as the new class then
					// add them
					if (qstrcmp(meta->className(), meta->enumerator(id).scope()) == 0) {
						for (int i = 0; i < meta->enumerator(id).keyCount(); i++) {
							rb_define_const(	klass, 
												meta->enumerator(id).key(i), 
												INT2NUM(meta->enumerator(id).value(i)) );
						}
					}
				}
			}

			// Add a Qt::Object.metaObject method which will do dynamic despatch on the
			// metaObject() virtual method so that the true QMetaObject of the class 
			// is returned, rather than for the one for the parent class that is in
			// the Smoke library.
			rb_define_method(klass, "metaObject", (VALUE (*) (...)) qobject_metaobject, 0);
		}
	}

    VALUE obj = Data_Wrap_Struct(klass, smokeruby_mark, smokeruby_free, (void *) o);
    return obj;
}

VALUE
cast_object_to(VALUE /*self*/, VALUE object, VALUE new_klass)
{
    smokeruby_object *o = value_obj_info(object);

	VALUE new_klassname = rb_funcall(new_klass, rb_intern("name"), 0);

    Smoke::ModuleIndex * cast_to_id = classcache.value(StringValuePtr(new_klassname));
	if (cast_to_id == 0) {
		rb_raise(rb_eArgError, "unable to find class \"%s\" to cast to\n", StringValuePtr(new_klassname));
	}

	smokeruby_object * o_cast = alloc_smokeruby_object(	o->allocated, 
														cast_to_id->smoke, 
														(int) cast_to_id->index, 
														o->smoke->cast(o->ptr, o->classId, (int) cast_to_id->index) );

    VALUE obj = Data_Wrap_Struct(new_klass, smokeruby_mark, smokeruby_free, (void *) o_cast);
    mapPointer(obj, o_cast, o_cast->classId, 0);
    return obj;
}

VALUE 
kross2smoke(VALUE /*self*/, VALUE krobject, VALUE new_klass)
{
  VALUE new_klassname = rb_funcall(new_klass, rb_intern("name"), 0);
  
  Smoke::ModuleIndex * cast_to_id = classcache.value(StringValuePtr(new_klassname));
  if (cast_to_id == 0) {
    rb_raise(rb_eArgError, "unable to find class \"%s\" to cast to\n", StringValuePtr(new_klassname));
  }
  
  void* o;
  Data_Get_Struct(krobject, void, o);
  
  smokeruby_object * o_cast = alloc_smokeruby_object(false, cast_to_id->smoke, (int) cast_to_id->index, o);
  
  VALUE obj = Data_Wrap_Struct(new_klass, smokeruby_mark, smokeruby_free, (void *) o_cast);
  mapPointer(obj, o_cast, o_cast->classId, 0);
  return obj;
}

VALUE
smoke2kross(VALUE /* self*/, VALUE sobj)
{
  smokeruby_object * o;
  Data_Get_Struct(sobj, smokeruby_object, o);	
  
  return Data_Wrap_Struct(rb_cObject, 0, 0, o->ptr );
}

VALUE
qvariant_value(VALUE /*self*/, VALUE variant_value_klass, VALUE variant_value)
{
	char * classname = rb_class2name(variant_value_klass);
    smokeruby_object *o = value_obj_info(variant_value);
	if (o == 0 || o->ptr == 0) {
		return Qnil;
	}

	QVariant * variant = (QVariant*) o->ptr;

    Smoke::ModuleIndex * value_class_id = classcache.value(classname);
	if (value_class_id == 0) {
		return Qnil;
	}

	void * value_ptr = 0;
	VALUE result = Qnil;
	smokeruby_object * vo = 0;

	if (qstrcmp(classname, "Qt::Pixmap") == 0) {
		QPixmap v = qVariantValue<QPixmap>(*variant);
		value_ptr = (void *) new QPixmap(v);
	} else if (qstrcmp(classname, "Qt::Font") == 0) {
		QFont v = qVariantValue<QFont>(*variant);
		value_ptr = (void *) new QFont(v);
	} else if (qstrcmp(classname, "Qt::Brush") == 0) {
		QBrush v = qVariantValue<QBrush>(*variant);
		value_ptr = (void *) new QBrush(v);
	} else if (qstrcmp(classname, "Qt::Color") == 0) {
		QColor v = qVariantValue<QColor>(*variant);
		value_ptr = (void *) new QColor(v);
	} else if (qstrcmp(classname, "Qt::Palette") == 0) {
		QPalette v = qVariantValue<QPalette>(*variant);
		value_ptr = (void *) new QPalette(v);
	} else if (qstrcmp(classname, "Qt::Icon") == 0) {
		QIcon v = qVariantValue<QIcon>(*variant);
		value_ptr = (void *) new QIcon(v);
	} else if (qstrcmp(classname, "Qt::Image") == 0) {
		QImage v = qVariantValue<QImage>(*variant);
		value_ptr = (void *) new QImage(v);
	} else if (qstrcmp(classname, "Qt::Polygon") == 0) {
		QPolygon v = qVariantValue<QPolygon>(*variant);
		value_ptr = (void *) new QPolygon(v);
	} else if (qstrcmp(classname, "Qt::Region") == 0) {
		QRegion v = qVariantValue<QRegion>(*variant);
		value_ptr = (void *) new QRegion(v);
	} else if (qstrcmp(classname, "Qt::Bitmap") == 0) {
		QBitmap v = qVariantValue<QBitmap>(*variant);
		value_ptr = (void *) new QBitmap(v);
	} else if (qstrcmp(classname, "Qt::Cursor") == 0) {
		QCursor v = qVariantValue<QCursor>(*variant);
		value_ptr = (void *) new QCursor(v);
	} else if (qstrcmp(classname, "Qt::SizePolicy") == 0) {
		QSizePolicy v = qVariantValue<QSizePolicy>(*variant);
		value_ptr = (void *) new QSizePolicy(v);
	} else if (qstrcmp(classname, "Qt::KeySequence") == 0) {
		QKeySequence v = qVariantValue<QKeySequence>(*variant);
		value_ptr = (void *) new QKeySequence(v);
	} else if (qstrcmp(classname, "Qt::Pen") == 0) {
		QPen v = qVariantValue<QPen>(*variant);
		value_ptr = (void *) new QPen(v);
	} else if (qstrcmp(classname, "Qt::TextLength") == 0) {
		QTextLength v = qVariantValue<QTextLength>(*variant);
		value_ptr = (void *) new QTextLength(v);
	} else if (qstrcmp(classname, "Qt::TextFormat") == 0) {
		QTextFormat v = qVariantValue<QTextFormat>(*variant);
		value_ptr = (void *) new QTextFormat(v);
	} else if (qstrcmp(classname, "Qt::Variant") == 0) {
		value_ptr = (void *) new QVariant(*((QVariant *) variant->constData()));
	} else if (variant->type() >= QVariant::UserType) { 
		value_ptr = QMetaType::construct(QMetaType::type(variant->typeName()), (void *) variant->constData());
	} else {
		// Assume the value of the Qt::Variant can be obtained
		// with a call such as Qt::Variant.toPoint()
		QByteArray toValueMethodName(classname);
		if (toValueMethodName.startsWith("Qt::")) {
			toValueMethodName.remove(0, strlen("Qt::"));
		}
		toValueMethodName.prepend("to");
		return rb_funcall(variant_value, rb_intern(toValueMethodName), 1, variant_value);
	}

	vo = alloc_smokeruby_object(true, value_class_id->smoke, value_class_id->index, value_ptr);
	result = set_obj_info(classname, vo);

	return result;
}

VALUE
qvariant_from_value(int argc, VALUE * argv, VALUE self)
{
	if (argc == 2) {
		Smoke::ModuleIndex nameId = qt_Smoke->NullModuleIndex;
		if (TYPE(argv[0]) == T_DATA) {
			nameId = qt_Smoke->idMethodName("QVariant#");
		} else if (TYPE(argv[0]) == T_ARRAY || TYPE(argv[0]) == T_ARRAY) {
			nameId = qt_Smoke->idMethodName("QVariant?");
		} else {
			nameId = qt_Smoke->idMethodName("QVariant$");
		}

		Smoke::ModuleIndex meth = qt_Smoke->findMethod(qt_Smoke->idClass("QVariant"), nameId);
		Smoke::Index i = meth.smoke->methodMaps[meth.index].method;
		i = -i;		// turn into ambiguousMethodList index
		while (meth.smoke->ambiguousMethodList[i] != 0) {
			if (	qstrcmp(	meth.smoke->types[meth.smoke->argumentList[meth.smoke->methods[meth.smoke->ambiguousMethodList[i]].args]].name,
								StringValuePtr(argv[1]) ) == 0 )
			{
				_current_method.smoke = meth.smoke;
				_current_method.index = meth.smoke->ambiguousMethodList[i];
				MethodCall c(meth.smoke, _current_method.index, self, argv, 0);
				c.next();
				return *(c.var());
			}

			i++;
		}
	}

	char * classname = rb_obj_classname(argv[0]);
    smokeruby_object *o = value_obj_info(argv[0]);
	if (o == 0 || o->ptr == 0) {
		// Assume the Qt::Variant can be created with a
		// Qt::Variant.new(obj) call
		if (qstrcmp(classname, "Qt::Enum") == 0) {
			return rb_funcall(qvariant_class, rb_intern("new"), 1, rb_funcall(argv[0], rb_intern("to_i"), 0));
		} else {
			return rb_funcall(qvariant_class, rb_intern("new"), 1, argv[0]);
		}
	}

	QVariant * v = 0;

	if (qstrcmp(classname, "Qt::Pixmap") == 0) {
		v = new QVariant(qVariantFromValue(*(QPixmap*) o->ptr));
	} else if (qstrcmp(classname, "Qt::Font") == 0) {
		v = new QVariant(qVariantFromValue(*(QFont*) o->ptr));
	} else if (qstrcmp(classname, "Qt::Brush") == 0) {
		v = new QVariant(qVariantFromValue(*(QBrush*) o->ptr));
	} else if (qstrcmp(classname, "Qt::Color") == 0) {
		v = new QVariant(qVariantFromValue(*(QColor*) o->ptr));
	} else if (qstrcmp(classname, "Qt::Palette") == 0) {
		v = new QVariant(qVariantFromValue(*(QPalette*) o->ptr));
	} else if (qstrcmp(classname, "Qt::Icon") == 0) {
		v = new QVariant(qVariantFromValue(*(QIcon*) o->ptr));
	} else if (qstrcmp(classname, "Qt::Image") == 0) {
		v = new QVariant(qVariantFromValue(*(QImage*) o->ptr));
	} else if (qstrcmp(classname, "Qt::Polygon") == 0) {
		v = new QVariant(qVariantFromValue(*(QPolygon*) o->ptr));
	} else if (qstrcmp(classname, "Qt::Region") == 0) {
		v = new QVariant(qVariantFromValue(*(QRegion*) o->ptr));
	} else if (qstrcmp(classname, "Qt::Bitmap") == 0) {
		v = new QVariant(qVariantFromValue(*(QBitmap*) o->ptr));
	} else if (qstrcmp(classname, "Qt::Cursor") == 0) {
		v = new QVariant(qVariantFromValue(*(QCursor*) o->ptr));
	} else if (qstrcmp(classname, "Qt::SizePolicy") == 0) {
		v = new QVariant(qVariantFromValue(*(QSizePolicy*) o->ptr));
	} else if (qstrcmp(classname, "Qt::KeySequence") == 0) {
		v = new QVariant(qVariantFromValue(*(QKeySequence*) o->ptr));
	} else if (qstrcmp(classname, "Qt::Pen") == 0) {
		v = new QVariant(qVariantFromValue(*(QPen*) o->ptr));
	} else if (qstrcmp(classname, "Qt::TextLength") == 0) {
		v = new QVariant(qVariantFromValue(*(QTextLength*) o->ptr));
	} else if (qstrcmp(classname, "Qt::TextFormat") == 0) {
		v = new QVariant(qVariantFromValue(*(QTextFormat*) o->ptr));
	} else if (QVariant::nameToType(o->smoke->classes[o->classId].className) >= QVariant::UserType) {
		v = new QVariant(QVariant::nameToType(o->smoke->classes[o->classId].className), o->ptr);
	} else {
		// Assume the Qt::Variant can be created with a
		// Qt::Variant.new(obj) call
		return rb_funcall(qvariant_class, rb_intern("new"), 1, argv[0]);
	}

	smokeruby_object * vo = alloc_smokeruby_object(true, o->smoke, o->smoke->findClass("QVariant").index, v);
	VALUE result = set_obj_info("Qt::Variant", vo);

	return result;
}

const char *
get_VALUEtype(VALUE ruby_value)
{
	char * classname = rb_obj_classname(ruby_value);
	const char *r = "";
	if (ruby_value == Qnil)
		r = "u";
	else if (TYPE(ruby_value) == T_FIXNUM || TYPE(ruby_value) == T_BIGNUM || qstrcmp(classname, "Qt::Integer") == 0)
		r = "i";
	else if (TYPE(ruby_value) == T_FLOAT)
		r = "n";
	else if (TYPE(ruby_value) == T_STRING)
		r = "s";
	else if(ruby_value == Qtrue || ruby_value == Qfalse || qstrcmp(classname, "Qt::Boolean") == 0)
		r = "B";
	else if (qstrcmp(classname, "Qt::Enum") == 0) {
		VALUE temp = rb_funcall(qt_internal_module, rb_intern("get_qenum_type"), 1, ruby_value);
		r = StringValuePtr(temp);
	} else if (TYPE(ruby_value) == T_DATA) {
		smokeruby_object *o = value_obj_info(ruby_value);
		if (o == 0 || o->smoke == 0) {
			r = "a";
		} else {
			r = o->smoke->classes[o->classId].className;
		}
	} else {
		r = "U";
	}

    return r;
}

VALUE prettyPrintMethod(Smoke::Index id) 
{
    VALUE r = rb_str_new2("");
    Smoke::Method &meth = qt_Smoke->methods[id];
    const char *tname = qt_Smoke->types[meth.ret].name;
    if(meth.flags & Smoke::mf_static) rb_str_catf(r, "static ");
    rb_str_catf(r, "%s ", (tname ? tname:"void"));
    rb_str_catf(r, "%s::%s(", qt_Smoke->classes[meth.classId].className, qt_Smoke->methodNames[meth.name]);
    for(int i = 0; i < meth.numArgs; i++) {
	if(i) rb_str_catf(r, ", ");
	tname = qt_Smoke->types[qt_Smoke->argumentList[meth.args+i]].name;
	rb_str_catf(r, "%s", (tname ? tname:"void"));
    }
    rb_str_catf(r, ")");
    if(meth.flags & Smoke::mf_const) rb_str_catf(r, " const");
    return r;
}
}
