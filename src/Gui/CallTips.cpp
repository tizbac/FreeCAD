/***************************************************************************
 *   Copyright (c) 2007 Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"

#ifndef _PreComp_
# include <QApplication>
# include <QKeyEvent>
# include <QLabel>
# include <QPlainTextEdit>
# include <QRegularExpression>
# include <QRegularExpressionMatch>
# include <QTextCursor>
# include <QToolTip>
# include <QScreen>
# if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
#   include <QDesktopWidget>
# endif
#endif

#include <App/Property.h>
#include <App/PropertyContainer.h>
#include <App/PropertyContainerPy.h>
#include <App/Document.h>
#include <App/DocumentObject.h>
#include <App/DocumentPy.h>
#include <App/DocumentObjectPy.h>
#include <Base/Console.h>
#include <Base/Interpreter.h>
#include <Base/PyObjectBase.h>
#include <Gui/BitmapFactory.h>
#include <Gui/DocumentPy.h>

#include "CallTips.h"


Q_DECLARE_METATYPE( Gui::CallTip ) //< allows use of QVariant

namespace Gui
{

/**
 * template class Temporary.
 * Allows variable changes limited to a scope.
 */
template <typename TYPE>
class Temporary
{
public:
    Temporary( TYPE &var, const TYPE tmpVal )
      : _var(var), _saveVal(var)
    { var = tmpVal; }

    ~Temporary( )
    { _var = _saveVal; }

private:
    TYPE &_var;
    TYPE  _saveVal;
};

} /* namespace Gui */

using namespace Gui;

CallTipsList::CallTipsList(QPlainTextEdit* parent)
  :  QListWidget(nullptr), textEdit(parent), cursorPos(0), validObject(true), doCallCompletion(false)
{
    this->setParent(0, Qt::Popup);
    this->setFocusPolicy(Qt::NoFocus);
    this->setFocusProxy(textEdit);

    // make the user assume that the widget is active
    QPalette pal = parent->palette();
    pal.setColor(QPalette::Inactive, QPalette::Highlight, pal.color(QPalette::Active, QPalette::Highlight));
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, pal.color(QPalette::Active, QPalette::HighlightedText));
    parent->setPalette( pal );

    connect(this, &QListWidget::itemActivated, this, &CallTipsList::callTipItemActivated);

    hideKeys.append(Qt::Key_Space);
    hideKeys.append(Qt::Key_Exclam);
    hideKeys.append(Qt::Key_QuoteDbl);
    hideKeys.append(Qt::Key_NumberSign);
    hideKeys.append(Qt::Key_Dollar);
    hideKeys.append(Qt::Key_Percent);
    hideKeys.append(Qt::Key_Ampersand);
    hideKeys.append(Qt::Key_Apostrophe);
    hideKeys.append(Qt::Key_Asterisk);
    hideKeys.append(Qt::Key_Plus);
    hideKeys.append(Qt::Key_Comma);
    hideKeys.append(Qt::Key_Minus);
    hideKeys.append(Qt::Key_Period);
    hideKeys.append(Qt::Key_Slash);
    hideKeys.append(Qt::Key_Colon);
    hideKeys.append(Qt::Key_Semicolon);
    hideKeys.append(Qt::Key_Less);
    hideKeys.append(Qt::Key_Equal);
    hideKeys.append(Qt::Key_Greater);
    hideKeys.append(Qt::Key_Question);
    hideKeys.append(Qt::Key_At);
    hideKeys.append(Qt::Key_Backslash);

    compKeys.append(Qt::Key_ParenLeft);
    compKeys.append(Qt::Key_ParenRight);
    compKeys.append(Qt::Key_BracketLeft);
    compKeys.append(Qt::Key_BracketRight);
    compKeys.append(Qt::Key_BraceLeft);
    compKeys.append(Qt::Key_BraceRight);
}

CallTipsList::~CallTipsList()
{
}

void CallTipsList::keyboardSearch(const QString& wordPrefix)
{
    // first search for the item that matches perfectly
    for (int i=0; i<count(); ++i) {
        QString text = item(i)->text();
        if (text.startsWith(wordPrefix)) {
            setCurrentRow(i);
            return;
        }
    }

    // now do a case insensitive comparison
    for (int i=0; i<count(); ++i) {
        QString text = item(i)->text();
        if (text.startsWith(wordPrefix, Qt::CaseInsensitive)) {
            setCurrentRow(i);
            return;
        }
    }

    if (currentItem())
        currentItem()->setSelected(false);
}

void CallTipsList::validateCursor()
{
    QTextCursor cursor = textEdit->textCursor();
    int currentPos = cursor.position();
    if (currentPos < this->cursorPos) {
        hide();
    }
    else {
        cursor.setPosition(this->cursorPos);
        cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
        QString word = cursor.selectedText();
        if (!word.isEmpty()) {
            // the following text might be an operator, brackets, ...
            const QChar underscore =  QLatin1Char('_');
            const QChar ch = word.at(0);
            if (ch.isSpace() || (ch != underscore && ch.isPunct()))
                word.clear();
        }
        if (currentPos > this->cursorPos+word.length()) {
            hide();
        }
        else if (!word.isEmpty()){
            // If the word is empty we should not allow to do a search,
            // otherwise we may select the next item which is not okay in this
            // context. This might happen if e.g. Shift is pressed.
            keyboardSearch(word);
        }
    }
}

QString CallTipsList::extractContext(const QString& line) const
{
    int len = line.size();
    int index = len-1;
    for (int i=0; i<len; i++) {
        int pos = len-1-i;
        const char ch = line.at(pos).toLatin1();
        if ((ch >= 48 && ch <= 57)  ||    // Numbers
            (ch >= 65 && ch <= 90)  ||    // Uppercase letters
            (ch >= 97 && ch <= 122) ||    // Lowercase letters
            (ch == '.') || (ch == '_') || // dot or underscore
            (ch == ' ') || (ch == '\t'))  // whitespace (between dot and text)
            index = pos;
        else
            break;
    }

    return line.mid(index);
}

QMap<QString, CallTip> CallTipsList::extractTips(const QString& context) const
{
    Base::PyGILStateLocker lock;
    QMap<QString, CallTip> tips;
    if (context.isEmpty())
        return tips;

    try {
        Py::Module module("__main__");
        Py::Dict dict = module.getDict();

        // this is used to filter out input of the form "1."
        QStringList items = context.split(QLatin1Char('.'));
        QString modname = items.front();
        items.pop_front();
        if (!dict.hasKey(std::string(modname.toUtf8())))
            return tips; // unknown object
        // Don't use hasattr & getattr because if a property is bound to a method this will be executed twice.
        PyObject* code = Py_CompileString(static_cast<const char*>(context.toUtf8()), "<CallTipsList>", Py_eval_input);
        if (!code) {
            PyErr_Clear();
            return tips;
        }

        PyObject* eval = nullptr;
        if (PyCode_Check(code)) {
            eval = PyEval_EvalCode(code, dict.ptr(), dict.ptr());
        }
        Py_DECREF(code);
        if (!eval) {
            PyErr_Clear();
            return tips;
        }
        Py::Object obj(eval, true);

        tips = extractTips(obj, &validObject);
    }
    catch (Py::Exception& e) {
        // Just clear the Python exception
        e.clear();
    }

    return tips;
}

QMap<QString, CallTip> CallTipsList::extractTips(Py::Object obj, bool *isValid) {
    QMap<QString, CallTip> tips;
    try {
        // Checks whether the type is a subclass of PyObjectBase because to get the doc string
        // of a member we must get it by its type instead of its instance otherwise we get the
        // wrong string, namely that of the type of the member.
        // Note: 3rd party libraries may use their own type object classes so that we cannot
        // reliably use Py::Type. To be on the safe side we should use Py::Object to assign
        // the used type object to.
        //Py::Object type = obj.type();
        Py::Object type(PyObject_Type(obj.ptr()), true);
        Py::Object inst = obj; // the object instance
        PyObject* typeobj = Base::getTypeAsObject(&Base::PyObjectBase::Type);
        PyObject* typedoc = Base::getTypeAsObject(&App::DocumentObjectPy::Type);
#ifdef FC_TIPS_FROM_TYPE
        PyObject* basetype = Base::getTypeAsObject(&PyBaseObject_Type);
#endif

        if (PyObject_IsSubclass(type.ptr(), typedoc) == 1) {
            // From the template Python object we don't query its type object because there we keep
            // a list of additional methods that we won't see otherwise. But to get the correct doc
            // strings we query the type's dict in the class itself.
            // To see if we have a template Python object we check for the existence of '__fc_template__'
            // See also: FeaturePythonPyT
            if (!obj.hasAttr("__fc_template__")) {
                obj = type;
            }
        }
        else if (PyObject_IsSubclass(type.ptr(), typeobj) == 1) {
            obj = type;
        }
#ifdef FC_TIPS_FROM_TYPE
        else if (PyObject_IsInstance(obj.ptr(), basetype) == 1) {
            // New style class which can be a module, type, list, tuple, int, float, ...
            // Make sure it's not a type object
            PyObject* typetype = Base::getTypeAsObject(&PyType_Type);
            if (PyObject_IsInstance(obj.ptr(), typetype) != 1) {
                // For wrapped objects with PySide2 use the object, not its type
                // as otherwise attributes added at runtime won't be listed (e.g. MainWindowPy)
                QString typestr(QString::fromUtf8(Py_TYPE(obj.ptr())->tp_name));

                // this should be now a user-defined Python class
                // http://stackoverflow.com/questions/12233103/in-python-at-runtime-determine-if-an-object-is-a-class-old-and-new-type-instan
                if (!typestr.startsWith(QStringLiteral("PySide")) && Py_TYPE(obj.ptr())->tp_flags & Py_TPFLAGS_HEAPTYPE) {
                    obj = type;
                }
            }
        }
#endif

        // If we have an instance of PyObjectBase then determine whether it's valid or not
        if (PyObject_IsInstance(inst.ptr(), typeobj) == 1) {
            Base::PyObjectBase* baseobj = static_cast<Base::PyObjectBase*>(inst.ptr());
            if(isValid)
                *isValid = baseobj->isValid();
        }
        else {
            // PyObject_IsInstance might set an exception
            PyErr_Clear();
        }

        // If we derive from PropertyContainerPy we can search for the properties in the
        // C++ twin class.
        PyObject* proptypeobj = Base::getTypeAsObject(&App::PropertyContainerPy::Type);
        if (PyObject_IsSubclass(type.ptr(), proptypeobj) == 1) {
            // These are the attributes of the instance itself which are NOT accessible by
            // its type object
            extractTipsFromProperties(inst, tips);
        }

        std::vector<std::string> list;

        // If we derive from App::DocumentPy we have direct access to the objects by their internal
        // names. So, we add these names to the list, too.
        PyObject* appdoctypeobj = Base::getTypeAsObject(&App::DocumentPy::Type);
        if (PyObject_IsSubclass(type.ptr(), appdoctypeobj) == 1) {
            auto docpy = static_cast<App::DocumentPy*>(inst.ptr());
            auto document = docpy->getDocumentPtr();
            // Make sure that the C++ object is alive
            if (document) {
                std::vector<App::DocumentObject*> objects = document->getObjects();
                list.clear();
                for (const auto & object : objects)
                    list.push_back(object->getNameInDocument());
                extractTipsFromObject(inst, list, tips);
            }
        }

        // If we derive from Gui::DocumentPy we have direct access to the objects by their internal
        // names. So, we add these names to the list, too.
        PyObject* guidoctypeobj = Base::getTypeAsObject(&Gui::DocumentPy::Type);
        if (PyObject_IsSubclass(type.ptr(), guidoctypeobj) == 1) {
            auto docpy = static_cast<Gui::DocumentPy*>(inst.ptr());
            if (docpy->getDocumentPtr()) {
                App::Document* document = docpy->getDocumentPtr()->getDocument();
                // Make sure that the C++ object is alive
                if (document) {
                    std::vector<App::DocumentObject*> objects = document->getObjects();
                    list.clear();
                    for (const auto & object : objects)
                        list.push_back(object->getNameInDocument());
                    extractTipsFromObject(inst, list, tips);
                }
            }
        }

        list.clear();
        Py::List attrList = obj.dir();
        for (Py::List::iterator it = attrList.begin(); it != attrList.end(); ++it) {
            Py::String attrname(*it);
            list.push_back(attrname.as_string());
        }

        // These are the attributes from the type object
        extractTipsFromObject(obj, list, tips);
    }
    catch (Py::Exception& e) {
        // Just clear the Python exception
        e.clear();
    }

    return tips;
}

void CallTipsList::extractTipsFromObject(Py::Object& obj, const std::vector<std::string> &list, QMap<QString, CallTip>& tips)
{
    for (const std::string &name : list) {
        try {
            // If 'name' is an invalid attribute then PyCXX raises an exception
            // for Py2 but silently accepts it for Py3.
            //
            // FIXME: Add methods of extension to the current instance and not its type object
            // https://forum.freecad.org/viewtopic.php?f=22&t=18105
            // https://forum.freecad.org/viewtopic.php?f=3&t=20009&p=154447#p154447
            // https://forum.freecad.org/viewtopic.php?f=10&t=12534&p=155290#p155290
            //
            // https://forum.freecad.org/viewtopic.php?f=39&t=33874&p=286759#p286759
            // https://forum.freecad.org/viewtopic.php?f=39&t=33874&start=30#p286772
            Py::Object attr = obj.getAttr(name);
            if (!attr.ptr()) {
                Base::Console().Log("Python attribute '%s' returns null!\n", name.c_str());
                continue;
            }

            CallTip tip;
            QString str = QString::fromUtf8(name.c_str());
            tip.name = str;

            if (attr.isCallable()) {
                PyObject* basetype = Base::getTypeAsObject(&PyBaseObject_Type);
                if (PyObject_IsSubclass(attr.ptr(), basetype) == 1) {
                    tip.type = CallTip::Class;
                }
                else {
                    PyErr_Clear(); // PyObject_IsSubclass might set an exception
                    tip.type = CallTip::Method;
                }
            }
            else if (PyModule_Check(attr.ptr())) {
                tip.type = CallTip::Module;
            }
            else {
                tip.type = CallTip::Member;
            }

            if (str == QStringLiteral("__doc__") && attr.isString()) {
                Py::Object help = attr;
                if (help.isString()) {
                    Py::String doc(help);
                    QString longdoc = QString::fromUtf8(doc.as_string().c_str());
                    int pos = longdoc.indexOf(QLatin1Char('\n'));
                    pos = qMin(pos, 70);
                    if (pos < 0)
                        pos = qMin(longdoc.length(), 70);
                    tip.description = stripWhiteSpace(longdoc);
                    tip.parameter = longdoc.left(pos);
                }
            }
            else if (attr.hasAttr("__doc__")) {
                Py::Object help = attr.getAttr("__doc__");
                if (help.isString()) {
                    Py::String doc(help);
                    QString longdoc = QString::fromUtf8(doc.as_string().c_str());
                    int pos = longdoc.indexOf(QLatin1Char('\n'));
                    pos = qMin(pos, 70);
                    if (pos < 0)
                        pos = qMin(longdoc.length(), 70);
                    tip.description = stripWhiteSpace(longdoc);
                    tip.parameter = longdoc.left(pos);
                }
            }

            // Do not override existing items
            QMap<QString, CallTip>::iterator pos = tips.find(str);
            if (pos == tips.end())
                tips[str] = tip;
        }
        catch (Py::Exception& e) {
            // Just clear the Python exception
            e.clear();
        }
    }
}

void CallTipsList::extractTipsFromProperties(Py::Object& obj, QMap<QString, CallTip>& tips)
{
    auto cont = static_cast<App::PropertyContainerPy*>(obj.ptr());
    App::PropertyContainer* container = cont->getPropertyContainerPtr();
    // Make sure that the C++ object is alive
    if (!container)
        return;

    std::vector<std::pair<const char*,App::Property*>> list;
    container->getPropertyNamedList(list);
    for (const auto & It : list) {
        CallTip tip;
        QString str = QString::fromUtf8(It.first);
        tip.name = str;
        tip.type = CallTip::Property;
        QString longdoc = QString::fromUtf8(It.second->getDocumentation());
        Py::Object data;
        // a point, mesh or shape property
        if (It.second->isDerivedFrom(Base::Type::fromName("App::PropertyComplexGeoData"))) {
            data = Py::Object(It.second->getPyObject(), true);
            if (data.hasAttr("__doc__")) {
                Py::Object help = data.getAttr("__doc__");
                if (help.isString()) {
                    Py::String doc(help);
                    longdoc = QString::fromUtf8(doc.as_string().c_str());
                }
            }
        }
        if (!longdoc.isEmpty()) {
            int pos = longdoc.indexOf(QLatin1Char('\n'));
            pos = qMin(pos, 70);
            if (pos < 0)
                pos = qMin(longdoc.length(), 70);
            tip.description = stripWhiteSpace(longdoc);
            tip.parameter = longdoc.left(pos);
        }
        tips[str] = tip;
    }
}

QIcon CallTipsList::iconOfType(CallTip::Type type, bool isValid)
{
    // search only once
    static QPixmap type_module_icon = BitmapFactory().pixmap("ClassBrowser/type_module.svg");
    static QPixmap type_class_icon = BitmapFactory().pixmap("ClassBrowser/type_class.svg");
    static QPixmap method_icon = BitmapFactory().pixmap("ClassBrowser/method.svg");
    static QPixmap member_icon = BitmapFactory().pixmap("ClassBrowser/member.svg");
    static QPixmap property_icon = BitmapFactory().pixmap("ClassBrowser/property.svg");

    // object is in error state
    static const char * const forbidden_xpm[]={
            "8 8 3 1",
            ". c None",
            "# c #ff0000",
            "a c #ffffff",
            "..####..",
            ".######.",
            "########",
            "#aaaaaa#",
            "#aaaaaa#",
            "########",
            ".######.",
            "..####.."};
    static QPixmap forbidden_icon(forbidden_xpm);
    static QPixmap forbidden_type_module_icon = BitmapFactory().merge(type_module_icon,forbidden_icon,BitmapFactoryInst::BottomLeft);
    static QPixmap forbidden_type_class_icon = BitmapFactory().merge(type_class_icon,forbidden_icon,BitmapFactoryInst::BottomLeft);
    static QPixmap forbidden_method_icon = BitmapFactory().merge(method_icon,forbidden_icon,BitmapFactoryInst::BottomLeft);
    static QPixmap forbidden_member_icon = BitmapFactory().merge(member_icon,forbidden_icon,BitmapFactoryInst::BottomLeft);
    static QPixmap forbidden_property_icon = BitmapFactory().merge(property_icon,forbidden_icon,BitmapFactoryInst::BottomLeft);

    switch (type)
    {
    case CallTip::Module:
        {
            return QIcon(isValid ? type_module_icon : forbidden_type_module_icon);
        }   break;
    case CallTip::Class:
        {
            return QIcon(isValid ? type_class_icon : forbidden_type_class_icon);
        }   break;
    case CallTip::Method:
        {
            return QIcon(isValid ? method_icon : forbidden_method_icon);
        }   break;
    case CallTip::Member:
        {
            return QIcon(isValid ? member_icon : forbidden_member_icon);
        }   break;
    case CallTip::Property:
        {
            return QIcon(isValid ? property_icon : forbidden_property_icon);
        }   break;
    default:
        return QIcon();
    }
}

void CallTipsList::showTips(const QString& line)
{
    this->validObject = true;
    QString context = extractContext(line);
    context = context.simplified();
    QMap<QString, CallTip> tips = extractTips(context);
    clear();
    for (QMap<QString, CallTip>::Iterator it = tips.begin(); it != tips.end(); ++it) {
        addItem(it.key());
        QListWidgetItem *item = this->item(this->count()-1);
        item->setData(Qt::ToolTipRole, QVariant(it.value().description));
        item->setData(Qt::UserRole, QVariant::fromValue( it.value() )); //< store full CallTip data
        QIcon icon = iconOfType(it.value().type);
        if(!icon.isNull())
            item->setIcon(icon);
    }

    if (count()==0)
        return; // nothing found

    // get the minimum width and height of the box
    int h = 0;
    int w = 0;
    // Hard code maximum visible item to 7, maybe parameterize it.
    for (int i = 0, c = std::min(7, count()); i < c; ++i) {
        QRect r = visualItemRect(item(i));
        w = qMax(w, r.width());
        h += r.height();
    }

    // Add an offset
    w += 2*frameWidth();
    h += 2*frameWidth();

    // get the start position of the word prefix
    QTextCursor cursor = textEdit->textCursor();
    this->cursorPos = cursor.position();
    QRect rect = textEdit->cursorRect(cursor);

#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QRect screen = QApplication::desktop()->availableGeometry(textEdit);
#else
    QRect screen = textEdit->screen()->availableGeometry();
#endif
    QPoint pos = textEdit->mapToGlobal(rect.topLeft());

    if (w > screen.width())
        w = screen.width();
    if ((pos.x() + w) > (screen.x() + screen.width()))
        pos.setX(screen.x() + screen.width() - w);
    if (pos.x() < screen.x())
        pos.setX(screen.x());

    int top = pos.y() - screen.top() + 2;
    int bottom = screen.bottom() - pos.y();
    h = qMax(h, this->minimumHeight());
    if (h > bottom) {
        h = qMin(qMax(top, bottom), h);

        if (top > bottom)
            pos.setY(pos.y() - h + 2);
    }
    setGeometry(pos.x(), pos.y(), w, h);

    setCurrentRow(0);
    show();
}

void CallTipsList::showEvent(QShowEvent* e)
{
    QListWidget::showEvent(e);
    // install this object to filter timer events for the tooltip label
    qApp->installEventFilter(this);
}

void CallTipsList::hideEvent(QHideEvent* e)
{
    QListWidget::hideEvent(e);
    qApp->removeEventFilter(this);
}

/**
 * Get all incoming events of the text edit and redirect some of them, like key up and
 * down, mouse press events, ... to the widget itself.
 */
bool CallTipsList::eventFilter(QObject * watched, QEvent * event)
{
    // This is a trick to avoid to hide the tooltip window after the defined time span
    // of 10 seconds. We just filter out all timer events to keep the label visible.
    if (watched->inherits("QLabel")) {
        auto label = qobject_cast<QLabel*>(watched);
        // Ignore the timer events to prevent from being closed
        if (label->windowFlags() & Qt::ToolTip && event->type() == QEvent::Timer)
            return true;
    }
    if (isVisible() && event->type() == QEvent::MouseButtonPress && !this->underMouse()) {
        this->hide();
        return true;
    }
    else if (isVisible() && (watched == textEdit || watched == this)) {
        if (event->type() == QEvent::InputMethod
                || event->type() == QEvent::InputMethodQuery)
        {
            if (watched == this) {
                QApplication::sendEvent(textEdit, event);
                return true;
            }
        }
        else if (event->type() == QEvent::KeyPress) {
            QKeyEvent* ke = (QKeyEvent*)event;
            if (ke->modifiers() != Qt::NoModifier &&
                    (ke->key() == Qt::Key_Space || ke->key() == Qt::Key_Tab))
            {
                return false;
            }
            if (watched == textEdit) {
                if (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down) {
                    keyPressEvent(ke);
                    return true;
                }
                else if (ke->key() == Qt::Key_PageUp || ke->key() == Qt::Key_PageDown) {
                    keyPressEvent(ke);
                    return true;
                }
                else if ((ke->key() == Qt::Key_Minus) && (ke->modifiers() & Qt::ShiftModifier)) {
                    // do nothing here, but this check is needed to ignore underscore
                    // which in Qt 4.8 gives Key_Minus instead of Key_Underscore
                }
            }
            if (ke->key() == Qt::Key_Escape) {
                hide();
                return true;
            }
            else if (this->hideKeys.indexOf(ke->key()) > -1) {
                Q_EMIT itemActivated(currentItem());
            }
            else if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                Q_EMIT itemActivated(currentItem());
                return true;
            }
            else if (ke->key() == Qt::Key_Tab) {
                // enable call completion for activating items
                Temporary<bool> tmp( this->doCallCompletion, true ); //< previous state restored on scope exit
                Q_EMIT itemActivated( currentItem() );
                return true;
            }
            else if (this->compKeys.indexOf(ke->key()) > -1) {
                Q_EMIT itemActivated(currentItem());
            }
            else if (ke->key() == Qt::Key_Shift || ke->key() == Qt::Key_Control ||
                     ke->key() == Qt::Key_Meta || ke->key() == Qt::Key_Alt ||
                     ke->key() == Qt::Key_AltGr) {
                // filter these meta keys to avoid to call keyboardSearch()
                return true;
            }
            if (watched == this) {
                qApp->sendEvent(textEdit, event);
                return true;
            }
        }
        else if (watched == textEdit && event->type() == QEvent::KeyRelease) {
            auto ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down ||
                ke->key() == Qt::Key_PageUp || ke->key() == Qt::Key_PageDown) {
                QList<QListWidgetItem *> items = selectedItems();
                if (!items.isEmpty()) {
                    QPoint p(width(), 0);
                    QString text = items.front()->toolTip();
                    if (!text.isEmpty()){
                        QToolTip::showText(mapToGlobal(p), text);
                    } else {
                        QToolTip::showText(p, QString());
                    }
                }
                return true;
            }
            qApp->sendEvent(textEdit, event);
            return true;
        }
        else if (event->type() == QEvent::FocusOut) {
            if (!hasFocus())
                hide();
        }
        else if (event->type() == QEvent::ShortcutOverride) {
            event->accept();
            return true;
        }
    }

    return QListWidget::eventFilter(watched, event);
}

void CallTipsList::callTipItemActivated(QListWidgetItem *item)
{
    hide();
    if (!item->isSelected())
        return;

    QString text = item->text();
    QTextCursor cursor = textEdit->textCursor();
    cursor.setPosition(this->cursorPos);
    cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    QString sel = cursor.selectedText();
    if (!sel.isEmpty()) {
        // in case the cursor moved too far on the right side
        const QChar underscore =  QLatin1Char('_');
        const QChar ch = sel.at(sel.count()-1);
        if (!ch.isLetterOrNumber() && ch != underscore)
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
    }
    cursor.insertText( text );

    // get CallTip from item's UserRole-data
    auto callTip = item->data(Qt::UserRole).value<CallTip>();

    // if call completion enabled and we've something callable (method or class constructor) ...
    if (this->doCallCompletion
     && (callTip.type == CallTip::Method || callTip.type == CallTip::Class))
    {
      cursor.insertText( QStringLiteral("()") ); //< just append parenthesis to identifier even inserted.

      /**
       * Try to find out if call needs arguments.
       * For this we search the description for appropriate hints ...
       */
      QRegularExpression argumentMatcher( QRegularExpression::escape( callTip.name ) + QStringLiteral("\\s*\\(\\s*\\w+.*\\)") );
      argumentMatcher.setPatternOptions( QRegularExpression::InvertedGreedinessOption ); //< set regex non-greedy!
      if (argumentMatcher.match( callTip.description ).hasMatch())
      {
        // if arguments are needed, we just move the cursor one left, to between the parentheses.
        cursor.movePosition( QTextCursor::Left, QTextCursor::MoveAnchor, 1 );
        textEdit->setTextCursor( cursor );
      }
    }
    textEdit->ensureCursorVisible();

    QRect rect = textEdit->cursorRect(cursor);
    int posX = rect.x();
    int posY = rect.y();

    QPoint p(posX, posY);
    p = textEdit->mapToGlobal(p);
    QToolTip::showText( p, callTip.parameter );
}

QString CallTipsList::stripWhiteSpace(const QString& str)
{
    QString stripped = str;
    QStringList lines = str.split(QStringLiteral("\n"));
    int minspace=INT_MAX;
    int line=0;
    for (QStringList::iterator it = lines.begin(); it != lines.end(); ++it, ++line) {
        if (it->count() > 0 && line > 0) {
            int space = 0;
            for (int i=0; i<it->count(); i++) {
                if ((*it)[i] == QLatin1Char('\t'))
                    space++;
                else
                    break;
            }

            if (it->count() > space)
                minspace = std::min<int>(minspace, space);
        }
    }

    // remove all leading tabs from each line
    if (minspace > 0 && minspace < INT_MAX) {
        int line=0;
        QStringList strippedlines;
        for (QStringList::iterator it = lines.begin(); it != lines.end(); ++it, ++line) {
            if (line == 0 && !it->isEmpty()) {
                strippedlines << *it;
            }
            else if (it->count() > 0 && line > 0) {
                strippedlines << it->mid(minspace);
            }
        }

        stripped = strippedlines.join(QStringLiteral("\n"));
    }

    return stripped;
}

#include "moc_CallTips.cpp"
