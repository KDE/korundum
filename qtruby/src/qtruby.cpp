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

#include <smoke/smoke.h>
#include <smoke/qt/qt_smoke.h>
#include <ruby.h>

#include "marshall_types.h"
#include "qtruby.h"

extern bool qRegisterResourceData(int, const unsigned char *, const unsigned char *, const unsigned char *);
extern bool qUnregisterResourceData(int, const unsigned char *, const unsigned char *, const unsigned char *);

extern TypeHandler Qt_handlers[];
extern const char * resolve_classname_qt(Smoke* smoke, int classId, void * ptr);

extern "C" {

static VALUE
qdebug(VALUE klass, VALUE msg)
{
	qDebug("%s", StringValuePtr(msg));
	return klass;
}

static VALUE
qfatal(VALUE klass, VALUE msg)
{
	qFatal("%s", StringValuePtr(msg));
	return klass;
}

static VALUE
qwarning(VALUE klass, VALUE msg)
{
	qWarning("%s", StringValuePtr(msg));
	return klass;
}

//---------- Ruby methods (for all functions except fully qualified statics & enums) ---------


// Takes a variable name and a QProperty with QVariant value, and returns a '
// variable=value' pair with the value in ruby inspect style
static QString
inspectProperty(QMetaProperty property, const char * name, QVariant & value)
{
	if (property.isEnumType()) {
		QMetaEnum e = property.enumerator();
		return QString(" %1=%2::%3").arg(name).arg(e.scope()).arg(e.valueToKey(value.toInt()));
	}
	
	switch (value.type()) {
	case QVariant::String:
	{
		if (value.toString().isNull()) {
			return QString(" %1=nil").arg(name); 
		} else {
			return QString(" %1=%2").arg(name).arg(value.toString());
		}
	}
		
	case QVariant::Bool:
	{
		QString rubyName;
		QRegExp name_re("^(is|has)(.)(.*)");
		
		if (name_re.indexIn(name) != -1) {
			rubyName = name_re.cap(2).toLower() + name_re.cap(3) + "?";
		} else {
			rubyName = name;
		}
		
		return QString(" %1=%2").arg(rubyName).arg(value.toString());
	}

	case QVariant::Color:
	{
		QColor c = value.value<QColor>();
		return QString(" %1=#<Qt::Color:0x0 %2>").arg(name).arg(c.name());
	}
			
	case QVariant::Cursor:
	{
		QCursor c = value.value<QCursor>();
		return QString(" %1=#<Qt::Cursor:0x0 shape=%2>").arg(name).arg(c.shape());
	}
	
	case QVariant::Double:
	{
		return QString(" %1=%2").arg(name).arg(value.toDouble());
	}
	
	case QVariant::Font:
	{
		QFont f = value.value<QFont>();
		return QString(	" %1=#<Qt::Font:0x0 family=%2, pointSize=%3, weight=%4, italic=%5, bold=%6, underline=%7, strikeOut=%8>")
									.arg(name)
									.arg(f.family())
									.arg(f.pointSize())
									.arg(f.weight())
									.arg(f.italic() ? "true" : "false")
									.arg(f.bold() ? "true" : "false")
									.arg(f.underline() ? "true" : "false")
									.arg(f.strikeOut() ? "true" : "false");
	}
	
	case QVariant::Line:
	{
		QLine l = value.toLine();
		return QString(" %1=#<Qt::Line:0x0 x1=%2, y1=%3, x2=%4, y2=%5>")
						.arg(name)
						.arg(l.x1())
						.arg(l.y1())
						.arg(l.x2())
						.arg(l.y2());
	}
	
	case QVariant::LineF:
	{
		QLineF l = value.toLineF();
		return QString(" %1=#<Qt::LineF:0x0 x1=%2, y1=%3, x2=%4, y2=%5>")
						.arg(name)
						.arg(l.x1())
						.arg(l.y1())
						.arg(l.x2())
						.arg(l.y2());
	}
	
	case QVariant::Point:
	{
		QPoint p = value.toPoint();
		return QString(" %1=#<Qt::Point:0x0 x=%2, y=%3>").arg(name).arg(p.x()).arg(p.y());
	}
	
	case QVariant::PointF:
	{
		QPointF p = value.toPointF();
		return QString(" %1=#<Qt::PointF:0x0 x=%2, y=%3>").arg(name).arg(p.x()).arg(p.y());
	}
	
	case QVariant::Rect:
	{
		QRect r = value.toRect();
		return QString(" %1=#<Qt::Rect:0x0 left=%2, right=%3, top=%4, bottom=%5>")
									.arg(name)
									.arg(r.left()).arg(r.right()).arg(r.top()).arg(r.bottom());
	}
	
	case QVariant::RectF:
	{
		QRectF r = value.toRectF();
		return QString(" %1=#<Qt::RectF:0x0 left=%2, right=%3, top=%4, bottom=%5>")
									.arg(name)
									.arg(r.left()).arg(r.right()).arg(r.top()).arg(r.bottom());
	}
	
	case QVariant::Size:
	{
		QSize s = value.toSize();
		return QString(" %1=#<Qt::Size:0x0 width=%2, height=%3>")
									.arg(name) 
									.arg(s.width()).arg(s.height());
	}
	
	case QVariant::SizeF:
	{
		QSizeF s = value.toSizeF();
		return QString(" %1=#<Qt::SizeF:0x0 width=%2, height=%3>")
									.arg(name) 
									.arg(s.width()).arg(s.height());
	}
	
	case QVariant::SizePolicy:
	{
		QSizePolicy s = value.value<QSizePolicy>();
		return QString(" %1=#<Qt::SizePolicy:0x0 horizontalPolicy=%2, verticalPolicy=%3>")
									.arg(name)
									.arg(s.horizontalPolicy())
									.arg(s.verticalPolicy());
	}
	
	case QVariant::Brush:
//	case QVariant::ColorGroup:
	case QVariant::Image:
	case QVariant::Palette:
	case QVariant::Pixmap:
	case QVariant::Region:
	{
		return QString(" %1=#<Qt::%2:0x0>").arg(name).arg(value.typeName() + 1);
	}
	
	default:
		return QString(" %1=%2").arg(name)
									.arg((value.isNull() || value.toString().isNull()) ? "nil" : value.toString() );
	}
}

// Retrieves the properties for a QObject and returns them as 'name=value' pairs
// in a ruby inspect string. For example:
//
//		#<Qt::HBoxLayout:0x30139030 name=unnamed, margin=0, spacing=0, resizeMode=3>
//
static VALUE
inspect_qobject(VALUE self)
{
	if (TYPE(self) != T_DATA) {
		return Qnil;
	}
	
	// Start with #<Qt::HBoxLayout:0x30139030> from the original inspect() call
	// Drop the closing '>'
	VALUE inspect_str = rb_call_super(0, 0);	
	rb_str_resize(inspect_str, RSTRING(inspect_str)->len - 1);
	
	smokeruby_object * o = 0;
    Data_Get_Struct(self, smokeruby_object, o);	
	QObject * qobject = (QObject *) o->smoke->cast(o->ptr, o->classId, o->smoke->idClass("QObject").index);
	
	QString value_list;
	value_list.append(QString(" objectName=\"%1\"").arg(qobject->objectName()));
	
	if (qobject->isWidgetType()) {
		QWidget * w = (QWidget *) qobject;
		value_list.append(QString(", x=%1, y=%2, width=%3, height=%4")
												.arg(w->x())
												.arg(w->y())
												.arg(w->width())
												.arg(w->height()) ); 
	}
		
	value_list.append(">");
	rb_str_cat2(inspect_str, value_list.toLatin1());
	
	return inspect_str;
}

// Retrieves the properties for a QObject and pretty_prints them as 'name=value' pairs
// For example:
//
//		#<Qt::HBoxLayout:0x30139030
//		 name=unnamed,
//		 margin=0,
//		 spacing=0,
//		 resizeMode=3>
//
static VALUE
pretty_print_qobject(VALUE self, VALUE pp)
{
	if (TYPE(self) != T_DATA) {
		return Qnil;
	}
	
	// Start with #<Qt::HBoxLayout:0x30139030>
	// Drop the closing '>'
	VALUE inspect_str = rb_funcall(self, rb_intern("to_s"), 0, 0);	
	rb_str_resize(inspect_str, RSTRING(inspect_str)->len - 1);
	rb_funcall(pp, rb_intern("text"), 1, inspect_str);
	rb_funcall(pp, rb_intern("breakable"), 0);
	
	smokeruby_object * o = 0;
    Data_Get_Struct(self, smokeruby_object, o);	
	QObject * qobject = (QObject *) o->smoke->cast(o->ptr, o->classId, o->smoke->idClass("QObject").index);
	
	QString value_list;		
	
	if (qobject->parent() != 0) {
		QString parentInspectString;
		VALUE obj = getPointerObject(qobject->parent());
		if (obj != Qnil) {
			VALUE parent_inspect_str = rb_funcall(obj, rb_intern("to_s"), 0, 0);	
			rb_str_resize(parent_inspect_str, RSTRING(parent_inspect_str)->len - 1);
			parentInspectString = StringValuePtr(parent_inspect_str);
		} else {
			parentInspectString.sprintf("#<%s:0x0", qobject->parent()->metaObject()->className());
		}
		
		if (qobject->parent()->isWidgetType()) {
			QWidget * w = (QWidget *) qobject->parent();
			value_list = QString("  parent=%1 objectName=\"%2\", x=%3, y=%4, width=%5, height=%6>,\n")
												.arg(parentInspectString)
												.arg(w->objectName())
												.arg(w->x())
												.arg(w->y())
												.arg(w->width())
												.arg(w->height());
		} else {
			value_list = QString("  parent=%1 objectName=\"%2\">,\n")
												.arg(parentInspectString)
												.arg(qobject->parent()->objectName());
		}
		
		rb_funcall(pp, rb_intern("text"), 1, rb_str_new2(value_list.toLatin1()));
	}
	
	if (qobject->children().count() != 0) {
		value_list = QString("  children=Array (%1 element(s)),\n")
								.arg(qobject->children().count());
		rb_funcall(pp, rb_intern("text"), 1, rb_str_new2(value_list.toLatin1()));
	}
	
	value_list = QString("  metaObject=#<Qt::MetaObject:0x0");
	value_list.append(QString(" className=%1").arg(qobject->metaObject()->className()));
	
	if (qobject->metaObject()->superClass() != 0) {
		value_list.append(	QString(", superClass=#<Qt::MetaObject:0x0 className=%1>")
							.arg(qobject->metaObject()->superClass()->className()) );
	}		
	
	value_list.append(">,\n");
	rb_funcall(pp, rb_intern("text"), 1, rb_str_new2(value_list.toLatin1()));

	QMetaProperty property = qobject->metaObject()->property(0);
	QVariant value = property.read(qobject);
	value_list = " " + inspectProperty(property, property.name(), value);
	rb_funcall(pp, rb_intern("text"), 1, rb_str_new2(value_list.toLatin1()));

	for (int index = 1; index < qobject->metaObject()->propertyCount(); index++) {
		rb_funcall(pp, rb_intern("text"), 1, rb_str_new2(",\n"));

		property = qobject->metaObject()->property(index);
		value = property.read(qobject);
		value_list = " " + inspectProperty(property, property.name(), value);
		rb_funcall(pp, rb_intern("text"), 1, rb_str_new2(value_list.toLatin1()));
	}

	rb_funcall(pp, rb_intern("text"), 1, rb_str_new2(">"));
	
	return self;
}

static VALUE
q_register_resource_data(VALUE /*self*/, VALUE version, VALUE tree_value, VALUE name_value, VALUE data_value)
{
	const unsigned char * tree = (const unsigned char *) malloc(RSTRING(tree_value)->len);
	memcpy((void *) tree, (const void *) RSTRING(tree_value)->ptr, RSTRING(tree_value)->len);

	const unsigned char * name = (const unsigned char *) malloc(RSTRING(name_value)->len);
	memcpy((void *) name, (const void *) RSTRING(name_value)->ptr, RSTRING(name_value)->len);

	const unsigned char * data = (const unsigned char *) malloc(RSTRING(data_value)->len);
	memcpy((void *) data, (const void *) RSTRING(data_value)->ptr, RSTRING(data_value)->len);

	return qRegisterResourceData(NUM2INT(version), tree, name, data) ? Qtrue : Qfalse;
}

static VALUE
q_unregister_resource_data(VALUE /*self*/, VALUE version, VALUE tree_value, VALUE name_value, VALUE data_value)
{
	const unsigned char * tree = (const unsigned char *) malloc(RSTRING(tree_value)->len);
	memcpy((void *) tree, (const void *) RSTRING(tree_value)->ptr, RSTRING(tree_value)->len);

	const unsigned char * name = (const unsigned char *) malloc(RSTRING(name_value)->len);
	memcpy((void *) name, (const void *) RSTRING(name_value)->ptr, RSTRING(name_value)->len);

	const unsigned char * data = (const unsigned char *) malloc(RSTRING(data_value)->len);
	memcpy((void *) data, (const void *) RSTRING(data_value)->ptr, RSTRING(data_value)->len);

	return qUnregisterResourceData(NUM2INT(version), tree, name, data) ? Qtrue : Qfalse;
}

static VALUE
qabstract_item_model_rowcount(int argc, VALUE * argv, VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;
	if (argc == 0) {
		return INT2NUM(model->rowCount());
	}

	if (argc == 1) {
		smokeruby_object * mi = value_obj_info(argv[0]);
		QModelIndex * modelIndex = (QModelIndex *) mi->ptr;
		return INT2NUM(model->rowCount(*modelIndex));
	}

	rb_raise(rb_eArgError, "Invalid argument list");
}

static VALUE
qabstract_item_model_columncount(int argc, VALUE * argv, VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;
	if (argc == 0) {
		return INT2NUM(model->columnCount());
	}

	if (argc == 1) {
		smokeruby_object * mi = value_obj_info(argv[0]);
		QModelIndex * modelIndex = (QModelIndex *) mi->ptr;
		return INT2NUM(model->columnCount(*modelIndex));
	}

	rb_raise(rb_eArgError, "Invalid argument list");
}

static VALUE
qabstract_item_model_data(int argc, VALUE * argv, VALUE self)
{
    smokeruby_object * o = value_obj_info(self);
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;
    smokeruby_object * mi = value_obj_info(argv[0]);
	QModelIndex * modelIndex = (QModelIndex *) mi->ptr;
	QVariant value;
	if (argc == 1) {
		value = model->data(*modelIndex);
	} else if (argc == 2) {
		value = model->data(*modelIndex, NUM2INT(rb_funcall(argv[1], rb_intern("to_i"), 0)));
	} else {
		rb_raise(rb_eArgError, "Invalid argument list");
	}


	smokeruby_object  * result = alloc_smokeruby_object(	true, 
															o->smoke, 
															o->smoke->findClass("QVariant").index, 
															new QVariant(value) );
	return set_obj_info("Qt::Variant", result);
}

static VALUE
qabstract_item_model_setdata(int argc, VALUE * argv, VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;
    smokeruby_object * mi = value_obj_info(argv[0]);
	QModelIndex * modelIndex = (QModelIndex *) mi->ptr;
    smokeruby_object * v = value_obj_info(argv[1]);
	QVariant * variant = (QVariant *) v->ptr;

	if (argc == 2) {
		return (model->setData(*modelIndex, *variant) ? Qtrue : Qfalse);
	}

	if (argc == 3) {
		return (model->setData(	*modelIndex, 
								*variant,
								NUM2INT(rb_funcall(argv[2], rb_intern("to_i"), 0)) ) ? Qtrue : Qfalse);
	}

	rb_raise(rb_eArgError, "Invalid argument list");
}

static VALUE
qabstract_item_model_flags(VALUE self, VALUE model_index)
{
    smokeruby_object *o = value_obj_info(self);
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;
    smokeruby_object * mi = value_obj_info(model_index);
	const QModelIndex * modelIndex = (const QModelIndex *) mi->ptr;
	return INT2NUM((int) model->flags(*modelIndex));
}

static VALUE
qabstract_item_model_insertrows(int argc, VALUE * argv, VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;

	if (argc == 2) {
		return (model->insertRows(NUM2INT(argv[0]), NUM2INT(argv[1])) ? Qtrue : Qfalse);
	}

	if (argc == 3) {
    	smokeruby_object * mi = value_obj_info(argv[2]);
		const QModelIndex * modelIndex = (const QModelIndex *) mi->ptr;
		return (model->insertRows(NUM2INT(argv[0]), NUM2INT(argv[1]), *modelIndex) ? Qtrue : Qfalse);
	}

	rb_raise(rb_eArgError, "Invalid argument list");
}

static VALUE
qabstract_item_model_insertcolumns(int argc, VALUE * argv, VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;

	if (argc == 2) {
		return (model->insertColumns(NUM2INT(argv[0]), NUM2INT(argv[1])) ? Qtrue : Qfalse);
	}

	if (argc == 3) {
    	smokeruby_object * mi = value_obj_info(argv[2]);
		const QModelIndex * modelIndex = (const QModelIndex *) mi->ptr;
		return (model->insertColumns(NUM2INT(argv[0]), NUM2INT(argv[1]), *modelIndex) ? Qtrue : Qfalse);
	}

	rb_raise(rb_eArgError, "Invalid argument list");
}

static VALUE
qabstract_item_model_removerows(int argc, VALUE * argv, VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;

	if (argc == 2) {
		return (model->removeRows(NUM2INT(argv[0]), NUM2INT(argv[1])) ? Qtrue : Qfalse);
	}

	if (argc == 3) {
    	smokeruby_object * mi = value_obj_info(argv[2]);
		const QModelIndex * modelIndex = (const QModelIndex *) mi->ptr;
		return (model->removeRows(NUM2INT(argv[0]), NUM2INT(argv[1]), *modelIndex) ? Qtrue : Qfalse);
	}

	rb_raise(rb_eArgError, "Invalid argument list");
}

static VALUE
qabstract_item_model_removecolumns(int argc, VALUE * argv, VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;

	if (argc == 2) {
		return (model->removeColumns(NUM2INT(argv[0]), NUM2INT(argv[1])) ? Qtrue : Qfalse);
	}

	if (argc == 3) {
    	smokeruby_object * mi = value_obj_info(argv[2]);
		const QModelIndex * modelIndex = (const QModelIndex *) mi->ptr;
		return (model->removeRows(NUM2INT(argv[0]), NUM2INT(argv[1]), *modelIndex) ? Qtrue : Qfalse);
	}

	rb_raise(rb_eArgError, "Invalid argument list");
}

// There is a QByteArray operator method in the Smoke lib that takes a QString
// arg and returns a QString. This is normally the desired behaviour, so
// special case a '+' method here.
static VALUE
qbytearray_append(VALUE self, VALUE str)
{
    smokeruby_object *o = value_obj_info(self);
	QByteArray * bytes = (QByteArray *) o->ptr;
	(*bytes) += (const char *) StringValuePtr(str);
	return self;
}

#ifdef QT_QTDBUS 
static VALUE
qdbusargument_endarraywrite(VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QDBusArgument * arg = (QDBusArgument *) o->ptr;
	arg->endArray();
	return self;
}

static VALUE
qdbusargument_endmapwrite(VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QDBusArgument * arg = (QDBusArgument *) o->ptr;
	arg->endMap();
	return self;
}

static VALUE
qdbusargument_endmapentrywrite(VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QDBusArgument * arg = (QDBusArgument *) o->ptr;
	arg->endMapEntry();
	return self;
}

static VALUE
qdbusargument_endstructurewrite(VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QDBusArgument * arg = (QDBusArgument *) o->ptr;
	arg->endStructure();
	return self;
}

static VALUE
qvariant_qdbusobjectpath_value(VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QVariant * arg = (QVariant *) o->ptr;
	QString s = qVariantValue<QDBusObjectPath>(*arg).path();
	return rb_str_new2(s.toLatin1());
}

static VALUE
qvariant_qdbussignature_value(VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QVariant * arg = (QVariant *) o->ptr;
	QString s = qVariantValue<QDBusSignature>(*arg).signature();
	return rb_str_new2(s.toLatin1());
}


#endif

// The QtRuby runtime's overloaded method resolution mechanism can't currently
// distinguish between Ruby Arrays containing different sort of instances.
// Unfortunately Qt::Painter.drawLines() and Qt::Painter.drawRects() methods can
// be passed a Ruby Array as an argument containing either Qt::Points or Qt::PointFs
// for instance. These methods need to call the correct Qt C++ methods, so special case
// the overload method resolution for now..
static VALUE
qpainter_drawlines(int argc, VALUE * argv, VALUE self)
{
static Smoke::Index drawlines_pointf_vector = 0;
static Smoke::Index drawlines_point_vector = 0;
static Smoke::Index drawlines_linef_vector = 0;
static Smoke::Index drawlines_line_vector = 0;

	if (argc == 1 && TYPE(argv[0]) == T_ARRAY && RARRAY(argv[0])->len > 0) {
		if (drawlines_point_vector == 0) {
			Smoke::ModuleIndex nameId = qt_Smoke->findMethodName("QPainter", "drawLines?");
			Smoke::ModuleIndex meth = qt_Smoke->findMethod(qt_Smoke->findClass("QPainter"), nameId);
			Smoke::Index i = meth.smoke->methodMaps[meth.index].method;
			i = -i;		// turn into ambiguousMethodList index
			while (meth.smoke->ambiguousMethodList[i] != 0) {
				const char * argType = meth.smoke->types[meth.smoke->argumentList[meth.smoke->methods[meth.smoke->ambiguousMethodList[i]].args]].name;

				if (qstrcmp(argType, "const QVector<QPointF>&" ) == 0) {
					drawlines_pointf_vector = meth.smoke->ambiguousMethodList[i];
				} else if (qstrcmp(argType, "const QVector<QPoint>&" ) == 0) {
					drawlines_point_vector = meth.smoke->ambiguousMethodList[i];
				} else if (qstrcmp(argType, "const QVector<QLineF>&" ) == 0) {
					drawlines_linef_vector = meth.smoke->ambiguousMethodList[i];
				} else if (qstrcmp(argType, "const QVector<QLine>&" ) == 0) {
					drawlines_line_vector = meth.smoke->ambiguousMethodList[i];
				}

				i++;
			}
		}

		smokeruby_object * o = value_obj_info(rb_ary_entry(argv[0], 0));

		if (qstrcmp(o->smoke->classes[o->classId].className, "QPointF") == 0) {
			_current_method.smoke = qt_Smoke;
			_current_method.index = drawlines_pointf_vector;
		} else if (qstrcmp(o->smoke->classes[o->classId].className, "QPoint") == 0) {
			_current_method.smoke = qt_Smoke;
			_current_method.index = drawlines_point_vector;
		} else if (qstrcmp(o->smoke->classes[o->classId].className, "QLineF") == 0) {
			_current_method.smoke = qt_Smoke;
			_current_method.index = drawlines_linef_vector;
		} else if (qstrcmp(o->smoke->classes[o->classId].className, "QLine") == 0) {
			_current_method.smoke = qt_Smoke;
			_current_method.index = drawlines_line_vector;
		} else {
			return rb_call_super(argc, argv);
		}

		MethodCall c(qt_Smoke, _current_method.index, self, argv, argc-1);
		c.next();
		return self;
	}

	return rb_call_super(argc, argv);
}

static VALUE
qpainter_drawrects(int argc, VALUE * argv, VALUE self)
{
static Smoke::Index drawlines_rectf_vector = 0;
static Smoke::Index drawlines_rect_vector = 0;

	if (argc == 1 && TYPE(argv[0]) == T_ARRAY && RARRAY(argv[0])->len > 0) {
		if (drawlines_rectf_vector == 0) {
			Smoke::ModuleIndex nameId = qt_Smoke->findMethodName("QPainter", "drawRects?");
			Smoke::ModuleIndex meth = qt_Smoke->findMethod(qt_Smoke->findClass("QPainter"), nameId);
			Smoke::Index i = meth.smoke->methodMaps[meth.index].method;
			i = -i;		// turn into ambiguousMethodList index
			while (meth.smoke->ambiguousMethodList[i] != 0) {
				const char * argType = meth.smoke->types[meth.smoke->argumentList[meth.smoke->methods[meth.smoke->ambiguousMethodList[i]].args]].name;

				if (qstrcmp(argType, "const QVector<QRectF>&" ) == 0) {
					drawlines_rectf_vector = meth.smoke->ambiguousMethodList[i];
				} else if (qstrcmp(argType, "const QVector<QRect>&" ) == 0) {
					drawlines_rect_vector = meth.smoke->ambiguousMethodList[i];
				}

				i++;
			}
		}

		smokeruby_object * o = value_obj_info(rb_ary_entry(argv[0], 0));

		if (qstrcmp(o->smoke->classes[o->classId].className, "QRectF") == 0) {
			_current_method.smoke = qt_Smoke;
			_current_method.index = drawlines_rectf_vector;
		} else if (qstrcmp(o->smoke->classes[o->classId].className, "QRect") == 0) {
			_current_method.smoke = qt_Smoke;
			_current_method.index = drawlines_rect_vector;
		} else {
			return rb_call_super(argc, argv);
		}

		MethodCall c(qt_Smoke, _current_method.index, self, argv, argc-1);
		c.next();
		return self;
	}

	return rb_call_super(argc, argv);
}

static VALUE
qabstractitemmodel_createindex(int argc, VALUE * argv, VALUE self)
{
	if (argc == 2 || argc == 3) {
		smokeruby_object * o = value_obj_info(self);
		Smoke::ModuleIndex nameId = o->smoke->idMethodName("createIndex$$$");
		Smoke::ModuleIndex meth = o->smoke->findMethod(qt_Smoke->findClass("QAbstractItemModel"), nameId);
		Smoke::Index i = meth.smoke->methodMaps[meth.index].method;
		i = -i;		// turn into ambiguousMethodList index
		while (o->smoke->ambiguousMethodList[i] != 0) {
			if (	qstrcmp(	o->smoke->types[o->smoke->argumentList[o->smoke->methods[o->smoke->ambiguousMethodList[i]].args + 2]].name,
							"void*" ) == 0 )
			{
	    		Smoke::Method &m = o->smoke->methods[o->smoke->ambiguousMethodList[i]];
				Smoke::ClassFn fn = o->smoke->classes[m.classId].classFn;
				Smoke::StackItem stack[4];
				stack[1].s_int = NUM2INT(argv[0]);
				stack[2].s_int = NUM2INT(argv[1]);
				if (argc == 2) {
					stack[3].s_voidp = (void*) Qnil;
				} else {
					stack[3].s_voidp = (void*) argv[2];
				}
				(*fn)(m.method, o->ptr, stack);
				smokeruby_object  * result = alloc_smokeruby_object(	true, 
																		o->smoke, 
																		o->smoke->idClass("QModelIndex").index, 
																		stack[0].s_voidp );

				return set_obj_info("Qt::ModelIndex", result);
			}

			i++;
		}
	}

	return rb_call_super(argc, argv);
}

static VALUE
qmodelindex_internalpointer(VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QModelIndex * index = (QModelIndex *) o->ptr;
	void * ptr = index->internalPointer();
	return ptr != 0 ? (VALUE) ptr : Qnil;
}

static VALUE
qitemselection_at(VALUE self, VALUE i)
{
    smokeruby_object *o = value_obj_info(self);
	QItemSelection * item = (QItemSelection *) o->ptr;
	QItemSelectionRange range = item->at(NUM2INT(i));

	smokeruby_object  * result = alloc_smokeruby_object(	true, 
															o->smoke, 
															o->smoke->idClass("QItemSelectionRange").index, 
															new QItemSelectionRange(range) );

	return set_obj_info("Qt::ItemSelectionRange", result);
}

static VALUE
qitemselection_count(VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
	QItemSelection * item = (QItemSelection *) o->ptr;
	return INT2NUM(item->count());
}

static VALUE
metaObject(VALUE self)
{
    VALUE metaObject = rb_funcall(qt_internal_module, rb_intern("getMetaObject"), 2, Qnil, self);
    return metaObject;
}

/* This shouldn't be needed, but kalyptus doesn't generate a staticMetaObject
	method for QObject::staticMetaObject, although it does for all the other
	classes, and it isn't obvious what the problem with it is. 
	So add this as a hack to work round the bug.
*/
static VALUE
qobject_staticmetaobject(VALUE /*klass*/)
{
	QMetaObject * meta = new QMetaObject(QObject::staticMetaObject);

	smokeruby_object  * m = alloc_smokeruby_object(	true, 
													qt_Smoke, 
													qt_Smoke->idClass("QMetaObject").index, 
													meta );

	VALUE obj = set_obj_info("Qt::MetaObject", m);
	return obj;
}

static VALUE
new_qvariant(int argc, VALUE * argv, VALUE self)
{
static Smoke::Index new_qvariant_qlist = 0;
static Smoke::Index new_qvariant_qmap = 0;

	if (new_qvariant_qlist == 0) {
		Smoke::ModuleIndex nameId = qt_Smoke->findMethodName("Qvariant", "QVariant?");
		Smoke::ModuleIndex meth = qt_Smoke->findMethod(qt_Smoke->findClass("QVariant"), nameId);
		Smoke::Index i = meth.smoke->methodMaps[meth.index].method;
		i = -i;		// turn into ambiguousMethodList index
		while (qt_Smoke->ambiguousMethodList[i] != 0) {
			const char * argType = meth.smoke->types[meth.smoke->argumentList[meth.smoke->methods[meth.smoke->ambiguousMethodList[i]].args]].name;

			if (qstrcmp(argType, "const QList<QVariant>&" ) == 0) {
				new_qvariant_qlist = meth.smoke->ambiguousMethodList[i];
			} else if (qstrcmp(argType, "const QMap<QString,QVariant>&" ) == 0) {
				new_qvariant_qmap = meth.smoke->ambiguousMethodList[i];
			}

			i++;
		}
	}

	if (argc == 1 && TYPE(argv[0]) == T_HASH) {
		_current_method.smoke = qt_Smoke;
		_current_method.index = new_qvariant_qmap;
		MethodCall c(qt_Smoke, _current_method.index, self, argv, argc-1);
		c.next();
    	return *(c.var());
	} else if (	argc == 1 
				&& TYPE(argv[0]) == T_ARRAY
				&& RARRAY(argv[0])->len > 0
				&& TYPE(rb_ary_entry(argv[0], 0)) != T_STRING )
	{
		_current_method.smoke = qt_Smoke;
		_current_method.index = new_qvariant_qlist;
		MethodCall c(qt_Smoke, _current_method.index, self, argv, argc-1);
		c.next();
		return *(c.var());
	}

	return rb_call_super(argc, argv);
}

static VALUE module_method_missing(int argc, VALUE * argv, VALUE /*klass*/)
{
    return class_method_missing(argc, argv, qt_module);
}

/*

class LCDRange < Qt::Widget

	def initialize(s, parent, name)
		super(parent, name)
		init()
		...

For a case such as the above, the QWidget can't be instantiated until
the initializer has been run up to the point where 'super(parent, name)'
is called. Only then, can the number and type of arguments passed to the
constructor be known. However, the rest of the intializer
can't be run until 'self' is a proper T_DATA object with a wrapped C++
instance.

The solution is to run the initialize code twice. First, only up to the
'super(parent, name)' call, where the QWidget would get instantiated in
initialize_qt(). And then rb_throw() jumps out of the
initializer returning the wrapped object as a result.

The second time round 'self' will be the wrapped instance of type T_DATA,
so initialize() can be allowed to proceed to the end.
*/
static VALUE
initialize_qt(int argc, VALUE * argv, VALUE self)
{
	VALUE retval = Qnil;
	VALUE temp_obj;
	
	if (TYPE(self) == T_DATA) {
		// If a ruby block was passed then run that now
		if (rb_block_given_p()) {
			rb_funcall(qt_internal_module, rb_intern("run_initializer_block"), 2, self, rb_block_proc());
		}

		return self;
	}

	VALUE klass = rb_funcall(self, rb_intern("class"), 0);
	VALUE constructor_name = rb_str_new2("new");

	VALUE * temp_stack = ALLOCA_N(VALUE, argc+4);

	temp_stack[0] = rb_str_new2("Qt");
	temp_stack[1] = constructor_name;
	temp_stack[2] = klass;
	temp_stack[3] = self;
	
	for (int count = 0; count < argc; count++) {
		temp_stack[count+4] = argv[count];
	}

	{ 
		QByteArray * mcid = find_cached_selector(argc+4, temp_stack, klass, rb_class2name(klass));

		if (_current_method.index == -1) {
			retval = rb_funcall2(qt_internal_module, rb_intern("do_method_missing"), argc+4, temp_stack);
			if (_current_method.index != -1) {
				// Success. Cache result.
				methcache.insert(*mcid, new Smoke::ModuleIndex(_current_method));
			}
		}
	}

	if (_current_method.index == -1) {
		// Another longjmp here..
		rb_raise(rb_eArgError, "unresolved constructor call %s\n", rb_class2name(klass));
	}
	
	{
		// Allocate the MethodCall within a C block. Otherwise, because the continue_new_instance()
		// call below will longjmp out, it wouldn't give C++ an opportunity to clean up
		MethodCall c(_current_method.smoke, _current_method.index, self, temp_stack+4, argc);
		c.next();
		temp_obj = *(c.var());
	}
	
	smokeruby_object * p = 0;
	Data_Get_Struct(temp_obj, smokeruby_object, p);

	smokeruby_object  * o = alloc_smokeruby_object(	true, 
													p->smoke, 
													p->classId, 
													p->ptr );
	p->ptr = 0;
	p->allocated = false;

	VALUE result = Data_Wrap_Struct(klass, smokeruby_mark, smokeruby_free, o);
	mapObject(result, result);
	// Off with a longjmp, never to return..
	rb_throw("newqt", result);
	/*NOTREACHED*/
	return self;
}

VALUE
new_qt(int argc, VALUE * argv, VALUE klass)
{
    VALUE * temp_stack = ALLOCA_N(VALUE, argc + 1);
	temp_stack[0] = rb_obj_alloc(klass);

	for (int count = 0; count < argc; count++) {
		temp_stack[count+1] = argv[count];
	}

	VALUE result = rb_funcall2(qt_internal_module, rb_intern("try_initialize"), argc+1, temp_stack);
	rb_obj_call_init(result, argc, argv);
	
	return result;
}

static VALUE
new_qapplication(int argc, VALUE * argv, VALUE klass)
{
 	VALUE result = Qnil;

	if (argc == 1 && TYPE(argv[0]) == T_ARRAY) {
		// Convert '(ARGV)' to '(NUM, [$0]+ARGV)'
		VALUE * local_argv = ALLOCA_N(VALUE, argc + 1);
		VALUE temp = rb_ary_dup(argv[0]);
		rb_ary_unshift(temp, rb_gv_get("$0"));
		local_argv[0] = INT2NUM(RARRAY(temp)->len);
		local_argv[1] = temp;
		result = new_qt(2, local_argv, klass);
	} else {
		result = new_qt(argc, argv, klass);
	}

	rb_gv_set("$qApp", result);
	return result;
}

// Returns $qApp.ARGV() - the original ARGV array with Qt command line options removed
static VALUE
qapplication_argv(VALUE /*self*/)
{
	VALUE result = rb_ary_new();
	// Drop argv[0], as it isn't included in the ruby global ARGV
	for (int index = 1; index < qApp->argc(); index++) {
		rb_ary_push(result, rb_str_new2(qApp->argv()[index]));
	}
	
	return result;
}

//----------------- Sig/Slot ------------------


static VALUE
qt_signal(int argc, VALUE * argv, VALUE self)
{
	smokeruby_object *o = value_obj_info(self);
	QObject *qobj = (QObject*)o->smoke->cast(o->ptr, o->classId, o->smoke->idClass("QObject").index);
    if (qobj->signalsBlocked()) {
		return Qfalse;
	}

	QLatin1String signalname(rb_id2name(rb_frame_last_func()));
	VALUE metaObject_value = rb_funcall(qt_internal_module, rb_intern("getMetaObject"), 2, Qnil, self);

	smokeruby_object *ometa = value_obj_info(metaObject_value);
	if (ometa == 0) {
		return Qnil;
	}

    int i = -1;
	const QMetaObject * m = (QMetaObject*) ometa->ptr;
    for (i = m->methodCount() - 1; i > -1; i--) {
		if (m->method(i).methodType() == QMetaMethod::Signal) {
			QString name(m->method(i).signature());
static QRegExp * rx = 0;
			if (rx == 0) {
				rx = new QRegExp("\\(.*");
			}
			name.replace(*rx, "");

			if (name == signalname) {
				break;
			}
		}
    }

	if (i == -1) {
		return Qnil;
	}

	QList<MocArgument*> args = get_moc_arguments(o->smoke, m->method(i).typeName(), m->method(i).parameterTypes());

	VALUE result = Qnil;
	// Okay, we have the signal info. *whew*
	EmitSignal signal(qobj, i, argc, args, argv, &result);
	signal.next();

	return result;
}

static VALUE
qt_metacall(int /*argc*/, VALUE * argv, VALUE self)
{
	// Arguments: QMetaObject::Call _c, int id, void ** _o
	QMetaObject::Call _c = (QMetaObject::Call) NUM2INT(	rb_funcall(	qt_internal_module,
																	rb_intern("get_qinteger"), 
																	1, 
																	argv[0] ) );
	int id = NUM2INT(argv[1]);
	void ** _o = 0;

	// Note that for a slot with no args and no return type,
	// it isn't an error to get a NULL value of _o here.
	Data_Get_Struct(argv[2], void*, _o);
	// Assume the target slot is a C++ one
	smokeruby_object *o = value_obj_info(self);
	Smoke::ModuleIndex nameId = o->smoke->idMethodName("qt_metacall$$?");
	Smoke::ModuleIndex classIdx = { o->smoke, o->classId };
	Smoke::ModuleIndex meth = nameId.smoke->findMethod(classIdx, nameId);
	if (meth.index > 0) {
		Smoke::Method &m = meth.smoke->methods[meth.smoke->methodMaps[meth.index].method];
		Smoke::ClassFn fn = meth.smoke->classes[m.classId].classFn;
		Smoke::StackItem i[4];
		i[1].s_enum = _c;
		i[2].s_int = id;
		i[3].s_voidp = _o;
		(*fn)(m.method, o->ptr, i);
		int ret = i[0].s_int;
		if (ret < 0) {
			return INT2NUM(ret);
		}
	} else {
		// Should never happen..
		rb_raise(rb_eRuntimeError, "Cannot find %s::qt_metacall() method\n", 
			o->smoke->classes[o->classId].className );
	}

    if (_c != QMetaObject::InvokeMetaMethod) {
		return argv[1];
	}

	QObject * qobj = (QObject *) o->smoke->cast(o->ptr, o->classId, o->smoke->idClass("QObject").index);
	// get obj metaobject with a virtual call
	const QMetaObject *metaobject = qobj->metaObject();

	// get method/property count
	int count = 0;
	if (_c == QMetaObject::InvokeMetaMethod) {
		count = metaobject->methodCount();
	} else {
		count = metaobject->propertyCount();
	}

	if (_c == QMetaObject::InvokeMetaMethod) {
		QMetaMethod method = metaobject->method(id);

		if (method.methodType() == QMetaMethod::Signal) {
			metaobject->activate(qobj, id, (void**) _o);
			return INT2NUM(id - count);
		}

		QList<MocArgument*> mocArgs = get_moc_arguments(o->smoke, method.typeName(), method.parameterTypes());

		QString name(method.signature());
static QRegExp * rx = 0;
		if (rx == 0) {
			rx = new QRegExp("\\(.*");
		}
		name.replace(*rx, "");
		InvokeSlot slot(self, rb_intern(name.toLatin1()), mocArgs, _o);
		slot.next();
	}
	
	return INT2NUM(id - count);
}

static VALUE
qobject_connect(int argc, VALUE * argv, VALUE self)
{
	if (rb_block_given_p()) {
		if (argc == 1) {
			return rb_funcall(qt_internal_module, rb_intern("signal_connect"), 3, self, argv[0], rb_block_proc());
		} else if (argc == 2) {
			return rb_funcall(qt_internal_module, rb_intern("connect"), 4, argv[0], argv[1], self, rb_block_proc());
		} else if (argc == 3) {
			return rb_funcall(qt_internal_module, rb_intern("connect"), 4, argv[0], argv[1], argv[2], rb_block_proc());
		} else {
			rb_raise(rb_eArgError, "Invalid argument list");
		}
	} else {
		return rb_call_super(argc, argv);
	}
}

static VALUE
qtimer_single_shot(int argc, VALUE * argv, VALUE /*self*/)
{
	if (rb_block_given_p()) {
		if (argc == 2) {
			return rb_funcall(qt_internal_module, rb_intern("single_shot_timer_connect"), 3, argv[0], argv[1], rb_block_proc());
		} else {
			rb_raise(rb_eArgError, "Invalid argument list");
		}
	} else {
		return rb_call_super(argc, argv);
	}
}

// --------------- Ruby C functions for Qt::_internal.* helpers  ----------------


static VALUE
getMethStat(VALUE /*self*/)
{
    VALUE result_list = rb_ary_new();
    rb_ary_push(result_list, INT2NUM((int)methcache.size()));
    rb_ary_push(result_list, INT2NUM((int)methcache.count()));
    return result_list;
}

static VALUE
getClassStat(VALUE /*self*/)
{
    VALUE result_list = rb_ary_new();
    rb_ary_push(result_list, INT2NUM((int)classcache.size()));
    rb_ary_push(result_list, INT2NUM((int)classcache.count()));
    return result_list;
}

static VALUE
getIsa(VALUE /*self*/, VALUE classId)
{
    VALUE parents_list = rb_ary_new();

    int id = NUM2INT(rb_funcall(classId, rb_intern("index"), 0));
    Smoke* smoke = smokeList[NUM2INT(rb_funcall(classId, rb_intern("smoke"), 0))];

    Smoke::Index *parents =
	smoke->inheritanceList +
	smoke->classes[id].parents;

    while(*parents) {
	//logger("\tparent: %s", qt_Smoke->classes[*parents].className);
	rb_ary_push(parents_list, rb_str_new2(smoke->classes[*parents++].className));
    }
    return parents_list;
}

// Return the class name of a QObject. Note that the name will be in the 
// form of Qt::Widget rather than QWidget. Is this a bug or a feature?
static VALUE
class_name(VALUE self)
{
    VALUE klass = rb_funcall(self, rb_intern("class"), 0);
    return rb_funcall(klass, rb_intern("name"), 0);
}

// Allow classnames in both 'Qt::Widget' and 'QWidget' formats to be
// used as an argument to Qt::Object.inherits()
static VALUE
inherits_qobject(int argc, VALUE * argv, VALUE /*self*/)
{
	if (argc != 1) {
		return rb_call_super(argc, argv);
	}

	Smoke::ModuleIndex * classId = classcache.value(StringValuePtr(argv[0]));

	if (classId == 0) {
		return rb_call_super(argc, argv);
	} else {
		VALUE super_class = rb_str_new2(classId->smoke->classes[classId->index].className);
		return rb_call_super(argc, &super_class);
	}
}

/* Adapted from the internal function qt_qFindChildren() in qobject.cpp */
static void 
rb_qFindChildren_helper(VALUE parent, const QString &name, VALUE re,
                         const QMetaObject &mo, VALUE list)
{
    if (parent == Qnil || list == Qnil)
        return;
	VALUE children = rb_funcall(parent, rb_intern("children"), 0);
    VALUE rv = Qnil;
    for (int i = 0; i < RARRAY(children)->len; ++i) {
        rv = RARRAY(children)->ptr[i];
		smokeruby_object *o = value_obj_info(rv);
		QObject * obj = (QObject *) o->smoke->cast(o->ptr, o->classId, o->smoke->idClass("QObject").index);
		
		// The original code had 'if (mo.cast(obj))' as a test, but it doesn't work here
        if (obj->qt_metacast(mo.className()) != 0) {
            if (re != Qnil) {
				VALUE re_test = rb_funcall(re, rb_intern("=~"), 1, rb_funcall(rv, rb_intern("objectName"), 0));
				if (re_test != Qnil && re_test != Qfalse) {
					rb_ary_push(list, rv);
				}
            } else {
                if (name.isNull() || obj->objectName() == name) {
					rb_ary_push(list, rv);
				}
            }
        }
        rb_qFindChildren_helper(rv, name, re, mo, list);
    }
	return;
}

/* Should mimic Qt4's QObject::findChildren method with this syntax:
     obj.findChildren(Qt::Widget, "Optional Widget Name")
*/
static VALUE
find_qobject_children(int argc, VALUE *argv, VALUE self)
{
	if (argc < 1 || argc > 2) rb_raise(rb_eArgError, "Invalid argument list");
	Check_Type(argv[0], T_CLASS);

	QString name;
	VALUE re = Qnil;
	if (argc == 2) {
		// If the second arg isn't a String, assume it's a regular expression
		if (TYPE(argv[1]) == T_STRING) {
			name = QString::fromLatin1(StringValuePtr(argv[1]));
		} else {
			re = argv[1];
		}
	}
		
	VALUE metaObject = rb_funcall(argv[0], rb_intern("staticMetaObject"), 0);
	smokeruby_object *o = value_obj_info(metaObject);
	QMetaObject * mo = (QMetaObject*) o->ptr;
	VALUE result = rb_ary_new();
	rb_qFindChildren_helper(self, name, re, *mo, result);
	return result;
}

/* Adapted from the internal function qt_qFindChild() in qobject.cpp */
static VALUE
rb_qFindChild_helper(VALUE parent, const QString &name, const QMetaObject &mo)
{
    if (parent == Qnil)
        return Qnil;
	VALUE children = rb_funcall(parent, rb_intern("children"), 0);
    VALUE rv;
	int i;
    for (i = 0; i < RARRAY(children)->len; ++i) {
        rv = RARRAY(children)->ptr[i];
		smokeruby_object *o = value_obj_info(rv);
		QObject * obj = (QObject *) o->smoke->cast(o->ptr, o->classId, o->smoke->idClass("QObject").index);
        if (obj->qt_metacast(mo.className()) != 0 && (name.isNull() || obj->objectName() == name))
            return rv;
    }
    for (i = 0; i < RARRAY(children)->len; ++i) {
        rv = rb_qFindChild_helper(RARRAY(children)->ptr[i], name, mo);
        if (rv != Qnil)
            return rv;
    }
    return Qnil;
}

static VALUE
find_qobject_child(int argc, VALUE *argv, VALUE self)
{
	if (argc < 1 || argc > 2) rb_raise(rb_eArgError, "Invalid argument list");
	Check_Type(argv[0], T_CLASS);

	QString name;
	if (argc == 2) {
		name = QString::fromLatin1(StringValuePtr(argv[1]));
	}
		
	VALUE metaObject = rb_funcall(argv[0], rb_intern("staticMetaObject"), 0);
	smokeruby_object *o = value_obj_info(metaObject);
	QMetaObject * mo = (QMetaObject*) o->ptr;
	return rb_qFindChild_helper(self, name, *mo);
}

static VALUE
setDebug(VALUE self, VALUE on_value)
{
    int on = NUM2INT(on_value);
    do_debug = on;
    return self;
}

static VALUE
debugging(VALUE /*self*/)
{
    return INT2NUM(do_debug);
}

static VALUE
getTypeNameOfArg(VALUE /*self*/, VALUE method_value, VALUE idx_value)
{
    int method = NUM2INT(rb_funcall(method_value, rb_intern("index"), 0));
    int smokeIndex = NUM2INT(rb_funcall(method_value, rb_intern("smoke"), 0));
    int idx = NUM2INT(idx_value);
    Smoke::Method &m = smokeList[smokeIndex]->methods[method];
    Smoke::Index *args = smokeList[smokeIndex]->argumentList + m.args;
    return rb_str_new2((char*)smokeList[smokeIndex]->types[args[idx]].name);
}

static VALUE
classIsa(VALUE /*self*/, VALUE className_value, VALUE base_value)
{
    char *className = StringValuePtr(className_value);
    char *base = StringValuePtr(base_value);
    return qt_Smoke->isDerivedFromByName(className, base) ? Qtrue : Qfalse;
}

static VALUE
isEnum(VALUE /*self*/, VALUE enumName_value)
{
    char *enumName = StringValuePtr(enumName_value);
    Smoke::Index typeId = 0;
    Smoke* s = 0;
    for (int i = 0; i < smokeList.count(); i++) {
         typeId = smokeList[i]->idType(enumName);
         if (typeId > 0) {
             s = smokeList[i];
             break;
         }
    }
	return	typeId > 0 
			&& (	(s->types[typeId].flags & Smoke::tf_elem) == Smoke::t_enum
					|| (s->types[typeId].flags & Smoke::tf_elem) == Smoke::t_ulong
					|| (s->types[typeId].flags & Smoke::tf_elem) == Smoke::t_long
					|| (s->types[typeId].flags & Smoke::tf_elem) == Smoke::t_uint
					|| (s->types[typeId].flags & Smoke::tf_elem) == Smoke::t_int ) ? Qtrue : Qfalse;
}

static VALUE
insert_pclassid(VALUE self, VALUE p_value, VALUE mi_value)
{
    char *p = StringValuePtr(p_value);
    int ix = NUM2INT(rb_funcall(mi_value, rb_intern("index"), 0));
    int smokeidx = NUM2INT(rb_funcall(mi_value, rb_intern("smoke"), 0));
    Smoke::ModuleIndex mi = { smokeList[smokeidx], ix };
    classcache.insert(QByteArray(p), new Smoke::ModuleIndex(mi));
    classname.insert(mi, new QByteArray(p));
    return self;
}

static VALUE
find_pclassid(VALUE /*self*/, VALUE p_value)
{
    char *p = StringValuePtr(p_value);
    Smoke::ModuleIndex *r = classcache.value(QByteArray(p));
    if(r)
        return rb_funcall(moduleindex_class, rb_intern("new"), 2, INT2NUM(smokeList.indexOf(r->smoke)), INT2NUM(r->index));
    else
        return rb_funcall(moduleindex_class, rb_intern("new"), 2, 0, 0);
}

static VALUE
getVALUEtype(VALUE /*self*/, VALUE ruby_value)
{
    return rb_str_new2(get_VALUEtype(ruby_value));
}

static QMetaObject* 
parent_meta_object(VALUE obj) 
{
	smokeruby_object* o = value_obj_info(obj);
	Smoke::ModuleIndex nameId = o->smoke->idMethodName("metaObject");
	Smoke::ModuleIndex classIdx = { o->smoke, o->classId };
	Smoke::ModuleIndex meth = o->smoke->findMethod(classIdx, nameId);
	if (meth.index <= 0) {
		// Should never happen..
	}

	Smoke::Method &methodId = meth.smoke->methods[meth.smoke->methodMaps[meth.index].method];
	Smoke::ClassFn fn = o->smoke->classes[methodId.classId].classFn;
	Smoke::StackItem i[1];
	(*fn)(methodId.method, o->ptr, i);
	return (QMetaObject*) i[0].s_voidp;
}

static VALUE
make_metaObject(VALUE /*self*/, VALUE obj, VALUE parentMeta, VALUE stringdata_value, VALUE data_value)
{
	QMetaObject* superdata = 0;

	if (parentMeta == Qnil) {
		// The parent class is a Smoke class, so call metaObject() on the
		// instance to get it via a smoke library call
		superdata = parent_meta_object(obj);
	} else {
		// The parent class is a custom Ruby class whose metaObject
		// was constructed at runtime
		smokeruby_object* p = value_obj_info(parentMeta);
		superdata = (QMetaObject *) p->ptr;
	}

	char *stringdata = new char[RSTRING(stringdata_value)->len];

	int count = RARRAY(data_value)->len;
	uint * data = new uint[count];

	memcpy(	(void *) stringdata, RSTRING(stringdata_value)->ptr, RSTRING(stringdata_value)->len );
	
	for (long i = 0; i < count; i++) {
		VALUE rv = rb_ary_entry(data_value, i);
		data[i] = NUM2UINT(rv);
	}
	
	QMetaObject ob = { 
		{ superdata, stringdata, data, 0 }
	} ;

	QMetaObject * meta = new QMetaObject;
	*meta = ob;

#ifdef DEBUG
	printf("make_metaObject() superdata: %p %s\n", meta->d.superdata, superdata->className());
	
	printf(
	" // content:\n"
	"       %d,       // revision\n"
	"       %d,       // classname\n"
	"       %d,   %d, // classinfo\n"
	"       %d,   %d, // methods\n"
	"       %d,   %d, // properties\n"
	"       %d,   %d, // enums/sets\n",
	data[0], data[1], data[2], data[3], 
	data[4], data[5], data[6], data[7], data[8], data[9]);

	int s = data[3];

	if (data[2] > 0) {
		printf("\n // classinfo: key, value\n");
		for (uint j = 0; j < data[2]; j++) {
			printf("      %d,    %d\n", data[s + (j * 2)], data[s + (j * 2) + 1]);
		}
	}

	s = data[5];
	bool signal_headings = true;
	bool slot_headings = true;

	for (uint j = 0; j < data[4]; j++) {
		if (signal_headings && (data[s + (j * 5) + 4] & 0x04) != 0) {
			printf("\n // signals: signature, parameters, type, tag, flags\n");
			signal_headings = false;
		} 

		if (slot_headings && (data[s + (j * 5) + 4] & 0x08) != 0) {
			printf("\n // slots: signature, parameters, type, tag, flags\n");
			slot_headings = false;
		}

		printf("      %d,   %d,   %d,   %d, 0x%2.2x\n", 
			data[s + (j * 5)], data[s + (j * 5) + 1], data[s + (j * 5) + 2], 
			data[s + (j * 5) + 3], data[s + (j * 5) + 4]);
	}

	s += (data[4] * 5);
	for (uint j = 0; j < data[6]; j++) {
		printf("\n // properties: name, type, flags\n");
		printf("      %d,   %d,   0x%8.8x\n", 
			data[s + (j * 3)], data[s + (j * 3) + 1], data[s + (j * 3) + 2]);
	}

	s += (data[6] * 3);
	for (int i = s; i < count; i++) {
		printf("\n       %d        // eod\n", data[i]);
	}

	printf("\nqt_meta_stringdata:\n    \"");

    int strlength = 0;
	for (int j = 0; j < RSTRING(stringdata_value)->len; j++) {
        strlength++;
		if (meta->d.stringdata[j] == 0) {
			printf("\\0");
			if (strlength > 40) {
				printf("\"\n    \"");
				strlength = 0;
			}
		} else {
			printf("%c", meta->d.stringdata[j]);
		}
	}
	printf("\"\n\n");

#endif
	smokeruby_object  * m = alloc_smokeruby_object(	true, 
													qt_Smoke, 
													qt_Smoke->idClass("QMetaObject").index, 
													meta );

    return Data_Wrap_Struct(qmetaobject_class, smokeruby_mark, smokeruby_free, m);
}

static VALUE
add_metaobject_methods(VALUE self, VALUE klass)
{
	rb_define_method(klass, "qt_metacall", (VALUE (*) (...)) qt_metacall, -1);
	rb_define_method(klass, "metaObject", (VALUE (*) (...)) metaObject, 0);
	return self;
}

static VALUE
add_signal_methods(VALUE self, VALUE klass, VALUE signalNames)
{
	for (long index = 0; index < RARRAY(signalNames)->len; index++) {
		VALUE signal = rb_ary_entry(signalNames, index);
		rb_define_method(klass, StringValuePtr(signal), (VALUE (*) (...)) qt_signal, -1);
	}
	return self;
}

static VALUE
dispose(VALUE self)
{
    smokeruby_object *o = value_obj_info(self);
    if(!o || !o->ptr) { return Qnil; }

    const char *className = o->smoke->classes[o->classId].className;
	if(do_debug & qtdb_gc) printf("Deleting (%s*)%p\n", className, o->ptr);
	
	unmapPointer(o, o->classId, 0);
	object_count--;

	char *methodName = new char[strlen(className) + 2];
	methodName[0] = '~';
	strcpy(methodName + 1, className);
	Smoke::ModuleIndex nameId = o->smoke->findMethodName(className, methodName);
	Smoke::ModuleIndex classIdx = { o->smoke, o->classId };
	Smoke::ModuleIndex meth = nameId.smoke->findMethod(classIdx, nameId);
	if(meth.index > 0) {
		Smoke::Method &m = meth.smoke->methods[meth.smoke->methodMaps[meth.index].method];
		Smoke::ClassFn fn = meth.smoke->classes[m.classId].classFn;
		Smoke::StackItem i[1];
		(*fn)(m.method, o->ptr, i);
	}
	delete[] methodName;
	o->ptr = 0;
	o->allocated = false;
	
	return self;
}

static VALUE
is_disposed(VALUE self)
{
	smokeruby_object *o = value_obj_info(self);
	if(o && o->ptr) { return Qtrue; }
	return Qfalse;
}

VALUE
isQObject(VALUE /*self*/, VALUE c)
{
    const char* classname = strdup(StringValuePtr(c));

    return qt_Smoke->isDerivedFromByName(classname, "QObject");

    free((void*) classname);
}

// Returns the Smoke classId of a ruby instance
static VALUE
idInstance(VALUE /*self*/, VALUE instance)
{
    smokeruby_object *o = value_obj_info(instance);
    if(!o)
        return Qnil;

    return rb_funcall(moduleindex_class, rb_intern("new"), 2, INT2NUM(smokeList.indexOf(o->smoke)), INT2NUM(o->classId));
}

static VALUE
findClass(VALUE /*self*/, VALUE name_value)
{
    char *name = StringValuePtr(name_value);
    Smoke::ModuleIndex mi = qt_Smoke->findClass(name);
    return rb_funcall(moduleindex_class, rb_intern("new"), 2, INT2NUM(smokeList.indexOf(mi.smoke)), INT2NUM(mi.index));
}

// static VALUE
// idMethodName(VALUE /*self*/, VALUE name_value)
// {
//     char *name = StringValuePtr(name_value);
//     return INT2NUM(qt_Smoke->idMethodName(name).index);
// }
// 
// static VALUE
// idMethod(VALUE /*self*/, VALUE idclass_value, VALUE idmethodname_value)
// {
//     int idclass = NUM2INT(idclass_value);
//     int idmethodname = NUM2INT(idmethodname_value);
//     return INT2NUM(qt_Smoke->idMethod(idclass, idmethodname).index);
// }

static VALUE
dumpCandidates(VALUE /*self*/, VALUE rmeths)
{
    VALUE errmsg = rb_str_new2("");
    if(rmeths != Qnil) {
	int count = RARRAY(rmeths)->len;
        for(int i = 0; i < count; i++) {
	    rb_str_catf(errmsg, "\t");
	    int id = NUM2INT(rb_funcall(rb_ary_entry(rmeths, i), rb_intern("index"), 0));
	    Smoke* smoke = smokeList[NUM2INT(rb_funcall(rb_ary_entry(rmeths, i), rb_intern("smoke"), 0))];
	    Smoke::Method &meth = smoke->methods[id];
	    const char *tname = smoke->types[meth.ret].name;
	    if(meth.flags & Smoke::mf_enum) {
			rb_str_catf(errmsg, "enum ");
			rb_str_catf(errmsg, "%s::%s", smoke->classes[meth.classId].className, smoke->methodNames[meth.name]);
			rb_str_catf(errmsg, "\n");
	    } else {
			if(meth.flags & Smoke::mf_static) rb_str_catf(errmsg, "static ");
			rb_str_catf(errmsg, "%s ", (tname ? tname:"void"));
			rb_str_catf(errmsg, "%s::%s(", smoke->classes[meth.classId].className, smoke->methodNames[meth.name]);
			for(int i = 0; i < meth.numArgs; i++) {
			if(i) rb_str_catf(errmsg, ", ");
			tname = smoke->types[smoke->argumentList[meth.args+i]].name;
			rb_str_catf(errmsg, "%s", (tname ? tname:"void"));
			}
			rb_str_catf(errmsg, ")");
			if(meth.flags & Smoke::mf_const) rb_str_catf(errmsg, " const");
			rb_str_catf(errmsg, "\n");
        	}
        }
    }
    return errmsg;
}

static VALUE
isObject(VALUE /*self*/, VALUE obj)
{
    void * ptr = 0;
    ptr = value_to_ptr(obj);
    return (ptr > 0 ? Qtrue : Qfalse);
}

static VALUE
setCurrentMethod(VALUE self, VALUE meth_value)
{
    int smokeidx = NUM2INT(rb_funcall(meth_value, rb_intern("smoke"), 0));
    int meth = NUM2INT(rb_funcall(meth_value, rb_intern("index"), 0));
    // FIXME: damn, this is lame, and it doesn't handle ambiguous methods
    _current_method.smoke = smokeList[smokeidx];  //qt_Smoke->methodMaps[meth].method;
    _current_method.index = meth;
    return self;
}

static VALUE
getClassList(VALUE /*self*/)
{
    VALUE class_list = rb_ary_new();

    for(int i = 1; i <= qt_Smoke->numClasses; i++) {
        if (qt_Smoke->classes[i].className)
            rb_ary_push(class_list, rb_str_new2(qt_Smoke->classes[i].className));
    }

    return class_list;
}

static VALUE
create_qobject_class(VALUE /*self*/, VALUE package_value, VALUE module_value)
{
	const char *package = strdup(StringValuePtr(package_value));
	// this won't work:
	// strdup(StringValuePtr(rb_funcall(module_value, rb_intern("name"), 0)))
	// any ideas why?
	VALUE value_moduleName = rb_funcall(module_value, rb_intern("name"), 0);
	const char *moduleName = strdup(StringValuePtr(value_moduleName));
	VALUE klass = module_value;
	
	QString packageName(package);

	foreach(QString s, packageName.mid(strlen(moduleName) + 2).split("::")) {
		klass = rb_define_class_under(klass, (const char*) s.toLatin1(), qt_base_class);
	}
	
	if (packageName == "Qt::Application" || packageName == "Qt::CoreApplication" ) {
		rb_define_singleton_method(klass, "new", (VALUE (*) (...)) new_qapplication, -1);
		rb_define_method(klass, "ARGV", (VALUE (*) (...)) qapplication_argv, 0);
	} else if (packageName == "Qt::Object") {
		rb_define_singleton_method(klass, "staticMetaObject", (VALUE (*) (...)) qobject_staticmetaobject, 0);
	} else if (packageName == "Qt::AbstractTableModel") {
		qtablemodel_class = rb_define_class_under(qt_module, "TableModel", klass);
		rb_define_method(qtablemodel_class, "rowCount", (VALUE (*) (...)) qabstract_item_model_rowcount, -1);
		rb_define_method(qtablemodel_class, "columnCount", (VALUE (*) (...)) qabstract_item_model_columncount, -1);
		rb_define_method(qtablemodel_class, "data", (VALUE (*) (...)) qabstract_item_model_data, -1);
		rb_define_method(qtablemodel_class, "setData", (VALUE (*) (...)) qabstract_item_model_setdata, -1);
		rb_define_method(qtablemodel_class, "flags", (VALUE (*) (...)) qabstract_item_model_flags, 1);
		rb_define_method(qtablemodel_class, "insertRows", (VALUE (*) (...)) qabstract_item_model_insertrows, -1);
		rb_define_method(qtablemodel_class, "insertColumns", (VALUE (*) (...)) qabstract_item_model_insertcolumns, -1);
		rb_define_method(qtablemodel_class, "removeRows", (VALUE (*) (...)) qabstract_item_model_removerows, -1);
		rb_define_method(qtablemodel_class, "removeColumns", (VALUE (*) (...)) qabstract_item_model_removecolumns, -1);
		
		qlistmodel_class = rb_define_class_under(qt_module, "ListModel", klass);
		rb_define_method(qlistmodel_class, "rowCount", (VALUE (*) (...)) qabstract_item_model_rowcount, -1);
		rb_define_method(qlistmodel_class, "columnCount", (VALUE (*) (...)) qabstract_item_model_columncount, -1);
		rb_define_method(qlistmodel_class, "data", (VALUE (*) (...)) qabstract_item_model_data, -1);
		rb_define_method(qlistmodel_class, "setData", (VALUE (*) (...)) qabstract_item_model_setdata, -1);
		rb_define_method(qlistmodel_class, "flags", (VALUE (*) (...)) qabstract_item_model_flags, 1);
		rb_define_method(qlistmodel_class, "insertRows", (VALUE (*) (...)) qabstract_item_model_insertrows, -1);
		rb_define_method(qlistmodel_class, "insertColumns", (VALUE (*) (...)) qabstract_item_model_insertcolumns, -1);
		rb_define_method(qlistmodel_class, "removeRows", (VALUE (*) (...)) qabstract_item_model_removerows, -1);
		rb_define_method(qlistmodel_class, "removeColumns", (VALUE (*) (...)) qabstract_item_model_removecolumns, -1);
	}
	else if (packageName == "Qt::AbstractItemModel") {
		rb_define_method(klass, "createIndex", (VALUE (*) (...)) qabstractitemmodel_createindex, -1);
	} else if (packageName == "Qt::Timer") {
		rb_define_singleton_method(klass, "singleShot", (VALUE (*) (...)) qtimer_single_shot, -1);
	}
	

	rb_define_method(klass, "inspect", (VALUE (*) (...)) inspect_qobject, 0);
	rb_define_method(klass, "pretty_print", (VALUE (*) (...)) pretty_print_qobject, 1);
	rb_define_method(klass, "className", (VALUE (*) (...)) class_name, 0);
	rb_define_method(klass, "inherits", (VALUE (*) (...)) inherits_qobject, -1);
	rb_define_method(klass, "findChildren", (VALUE (*) (...)) find_qobject_children, -1);
	rb_define_method(klass, "findChild", (VALUE (*) (...)) find_qobject_child, -1);   
	rb_define_method(klass, "connect", (VALUE (*) (...)) qobject_connect, -1);   
	rb_define_singleton_method(klass, "connect", (VALUE (*) (...)) qobject_connect, -1);   

	foreach(QtRubyModule m, qtruby_modules.values()) {
		if (m.class_created)
			m.class_created(package, module_value, klass);
	}

	free((void *) package);
	return klass;
}

static VALUE
create_qt_class(VALUE /*self*/, VALUE package_value, VALUE module_value)
{
	const char *package = strdup(StringValuePtr(package_value));
	// this won't work:
	// strdup(StringValuePtr(rb_funcall(module_value, rb_intern("name"), 0)))
	// any ideas why?
	VALUE value_moduleName = rb_funcall(module_value, rb_intern("name"), 0);
	const char *moduleName = strdup(StringValuePtr(value_moduleName));
	VALUE klass = module_value;
	QString packageName(package);
	if (packageName == "Qt::TextEdit::ExtraSelection") return Qnil;
	
	foreach(QString s, packageName.mid(strlen(moduleName) + 2).split("::")) {
		klass = rb_define_class_under(klass, (const char*) s.toLatin1(), qt_base_class);
	}

	if (packageName == "Qt::MetaObject") {
		qmetaobject_class = klass;
	} else if (packageName == "Qt::Variant") {
		qvariant_class = klass;
		rb_define_singleton_method(qvariant_class, "fromValue", (VALUE (*) (...)) qvariant_from_value, -1);
    	rb_define_singleton_method(qvariant_class, "new", (VALUE (*) (...)) new_qvariant, -1);
#ifdef QT_QTDBUS 
		rb_define_method(klass, "qdbusobjectpath_value", (VALUE (*) (...)) qvariant_qdbusobjectpath_value, 1);
		rb_define_method(klass, "qdbussignature_value", (VALUE (*) (...)) qvariant_qdbussignature_value, 1);
#endif
	} else if (packageName == "Qt::ByteArray") {
		rb_define_method(klass, "+", (VALUE (*) (...)) qbytearray_append, 1);
	} else if (packageName == "Qt::Char") {
		rb_define_method(klass, "to_s", (VALUE (*) (...)) qchar_to_s, 0);
	} else if (packageName == "Qt::ItemSelection") {
		rb_define_method(klass, "[]", (VALUE (*) (...)) qitemselection_at, 1);
		rb_define_method(klass, "at", (VALUE (*) (...)) qitemselection_at, 1);
		rb_define_method(klass, "count", (VALUE (*) (...)) qitemselection_count, 0);
		rb_define_method(klass, "length", (VALUE (*) (...)) qitemselection_count, 0);
	} else if (packageName == "Qt::Painter") {
		rb_define_method(klass, "drawLines", (VALUE (*) (...)) qpainter_drawlines, -1);
		rb_define_method(klass, "drawRects", (VALUE (*) (...)) qpainter_drawrects, -1);
	} else if (packageName == "Qt::ModelIndex") {
		rb_define_method(klass, "internalPointer", (VALUE (*) (...)) qmodelindex_internalpointer, 0);
#ifdef QT_QTDBUS
	} else if (packageName == "Qt::DBusArgument") {
		rb_define_method(klass, "endArrayWrite", (VALUE (*) (...)) qdbusargument_endarraywrite, 0);
		rb_define_method(klass, "endMapEntryWrite", (VALUE (*) (...)) qdbusargument_endmapentrywrite, 0);
		rb_define_method(klass, "endMapWrite", (VALUE (*) (...)) qdbusargument_endmapwrite, 0);
		rb_define_method(klass, "endStructureWrite", (VALUE (*) (...)) qdbusargument_endstructurewrite, 0);
#endif
	}

	foreach(QtRubyModule m, qtruby_modules.values()) {
		if (m.class_created)
			m.class_created(package, module_value, klass);
	}

	free((void *) package);
	return klass;
}

static VALUE
version(VALUE /*self*/)
{
    return rb_str_new2(QT_VERSION_STR);
}

static VALUE
qtruby_version(VALUE /*self*/)
{
    return rb_str_new2(QTRUBY_VERSION);
}

static VALUE
set_application_terminated(VALUE /*self*/, VALUE yn)
{
    application_terminated = (yn == Qtrue ? true : false);
	return Qnil;
}

extern Q_DECL_EXPORT void
Init_qtruby4()
{
// 	if (qt_Smoke != 0L) {
// 		// This function must have been called twice because both
// 		// 'require Qt' and 'require Korundum' statements have
// 		// been included in a ruby program
// 		rb_fatal("require 'Qt' must not follow require 'Korundum'\n");
// 		return;
// 	}

    if (qt_Smoke == 0) init_qt_Smoke();
    smokeList << qt_Smoke;
    qt_Smoke->binding = new QtRubySmokeBinding(qt_Smoke);

    QtRubyModule module = { "QtRuby", resolve_classname_qt, 0 };
    qtruby_modules[qt_Smoke] = module;

    install_handlers(Qt_handlers);

	if (qt_module == Qnil) {
		qt_module = rb_define_module("Qt");
		qt_internal_module = rb_define_module_under(qt_module, "Internal");
		qt_base_class = rb_define_class_under(qt_module, "Base", rb_cObject);
		moduleindex_class = rb_define_class_under(qt_internal_module, "ModuleIndex", rb_cObject);
	}

    rb_define_singleton_method(qt_base_class, "new", (VALUE (*) (...)) new_qt, -1);
    rb_define_method(qt_base_class, "initialize", (VALUE (*) (...)) initialize_qt, -1);
    rb_define_singleton_method(qt_base_class, "method_missing", (VALUE (*) (...)) class_method_missing, -1);
    rb_define_singleton_method(qt_module, "method_missing", (VALUE (*) (...)) module_method_missing, -1);
    rb_define_method(qt_base_class, "method_missing", (VALUE (*) (...)) method_missing, -1);

    rb_define_singleton_method(qt_base_class, "const_missing", (VALUE (*) (...)) class_method_missing, -1);
    rb_define_singleton_method(qt_module, "const_missing", (VALUE (*) (...)) module_method_missing, -1);
    rb_define_method(qt_base_class, "const_missing", (VALUE (*) (...)) method_missing, -1);

    rb_define_method(qt_base_class, "dispose", (VALUE (*) (...)) dispose, 0);
    rb_define_method(qt_base_class, "isDisposed", (VALUE (*) (...)) is_disposed, 0);
    rb_define_method(qt_base_class, "disposed?", (VALUE (*) (...)) is_disposed, 0);

	rb_define_method(qt_base_class, "qVariantValue", (VALUE (*) (...)) qvariant_value, 2);
	rb_define_method(qt_base_class, "qVariantFromValue", (VALUE (*) (...)) qvariant_from_value, -1);
    
	rb_define_method(rb_cObject, "qDebug", (VALUE (*) (...)) qdebug, 1);
	rb_define_method(rb_cObject, "qFatal", (VALUE (*) (...)) qfatal, 1);
	rb_define_method(rb_cObject, "qWarning", (VALUE (*) (...)) qwarning, 1);

    rb_define_module_function(qt_internal_module, "getMethStat", (VALUE (*) (...)) getMethStat, 0);
    rb_define_module_function(qt_internal_module, "getClassStat", (VALUE (*) (...)) getClassStat, 0);
    rb_define_module_function(qt_internal_module, "getIsa", (VALUE (*) (...)) getIsa, 1);
    rb_define_module_function(qt_internal_module, "setDebug", (VALUE (*) (...)) setDebug, 1);
    rb_define_module_function(qt_internal_module, "debug", (VALUE (*) (...)) debugging, 0);
    rb_define_module_function(qt_internal_module, "getTypeNameOfArg", (VALUE (*) (...)) getTypeNameOfArg, 2);
    rb_define_module_function(qt_internal_module, "classIsa", (VALUE (*) (...)) classIsa, 2);
    rb_define_module_function(qt_internal_module, "isEnum", (VALUE (*) (...)) isEnum, 1);
    rb_define_module_function(qt_internal_module, "insert_pclassid", (VALUE (*) (...)) insert_pclassid, 2);
    rb_define_module_function(qt_internal_module, "find_pclassid", (VALUE (*) (...)) find_pclassid, 1);
    rb_define_module_function(qt_internal_module, "getVALUEtype", (VALUE (*) (...)) getVALUEtype, 1);

    rb_define_module_function(qt_internal_module, "make_metaObject", (VALUE (*) (...)) make_metaObject, 4);
    rb_define_module_function(qt_internal_module, "addMetaObjectMethods", (VALUE (*) (...)) add_metaobject_methods, 1);
    rb_define_module_function(qt_internal_module, "addSignalMethods", (VALUE (*) (...)) add_signal_methods, 2);
    rb_define_module_function(qt_internal_module, "mapObject", (VALUE (*) (...)) mapObject, 1);

    rb_define_module_function(qt_internal_module, "isQObject", (VALUE (*) (...)) isQObject, 1);
    rb_define_module_function(qt_internal_module, "idInstance", (VALUE (*) (...)) idInstance, 1);
    rb_define_module_function(qt_internal_module, "findClass", (VALUE (*) (...)) findClass, 1);
//     rb_define_module_function(qt_internal_module, "idMethodName", (VALUE (*) (...)) idMethodName, 1);
//     rb_define_module_function(qt_internal_module, "idMethod", (VALUE (*) (...)) idMethod, 2);
    rb_define_module_function(qt_internal_module, "findMethod", (VALUE (*) (...)) findMethod, 2);
    rb_define_module_function(qt_internal_module, "findAllMethods", (VALUE (*) (...)) findAllMethods, -1);
    rb_define_module_function(qt_internal_module, "findAllMethodNames", (VALUE (*) (...)) findAllMethodNames, 3);
    rb_define_module_function(qt_internal_module, "dumpCandidates", (VALUE (*) (...)) dumpCandidates, 1);
    rb_define_module_function(qt_internal_module, "isObject", (VALUE (*) (...)) isObject, 1);
    rb_define_module_function(qt_internal_module, "setCurrentMethod", (VALUE (*) (...)) setCurrentMethod, 1);
    rb_define_module_function(qt_internal_module, "getClassList", (VALUE (*) (...)) getClassList, 0);
    rb_define_module_function(qt_internal_module, "create_qt_class", (VALUE (*) (...)) create_qt_class, 2);
    rb_define_module_function(qt_internal_module, "create_qobject_class", (VALUE (*) (...)) create_qobject_class, 2);
    rb_define_module_function(qt_internal_module, "cast_object_to", (VALUE (*) (...)) cast_object_to, 2);
    rb_define_module_function(qt_internal_module, "kross2smoke", (VALUE (*) (...)) kross2smoke, 2);
    rb_define_module_function(qt_internal_module, "smoke2kross", (VALUE (*) (...)) smoke2kross, 1);
    rb_define_module_function(qt_internal_module, "application_terminated=", (VALUE (*) (...)) set_application_terminated, 1);
    
	rb_define_module_function(qt_module, "version", (VALUE (*) (...)) version, 0);
    rb_define_module_function(qt_module, "qtruby_version", (VALUE (*) (...)) qtruby_version, 0);

    rb_define_module_function(qt_module, "qRegisterResourceData", (VALUE (*) (...)) q_register_resource_data, 4);
    rb_define_module_function(qt_module, "qUnregisterResourceData", (VALUE (*) (...)) q_unregister_resource_data, 4);

	rb_require("Qt/qtruby4.rb");

    // Do package initialization
    rb_funcall(qt_internal_module, rb_intern("init_all_classes"), 0);
}

}