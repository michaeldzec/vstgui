#include "uidialogcontroller.h"

#if VSTGUI_LIVE_EDITING

#include "uieditcontroller.h"
#include "../../lib/coffscreencontext.h"
#include "../../lib/controls/ctextlabel.h"
#include "../../lib/controls/cbuttons.h"
#include <cmath>

namespace VSTGUI {

namespace BoxBlur {

//----------------------------------------------------------------------------------------------------
static void calculateBlurColor (CColor& color, CColor* colors, int32_t numColors)
{
	int32_t red = 0;
	int32_t green = 0;
	int32_t blue = 0;
	int32_t alpha = 0;
	for (int32_t i = numColors-1; i >= 0; i--)
	{
		red += colors[i].red;
		green += colors[i].green;
		blue += colors[i].blue;
		alpha += colors[i].alpha;
		if (i+1 < numColors)
			colors[i+1] = colors[i];
	}
	red /= numColors;
	green /= numColors;
	blue /= numColors;
	alpha /= numColors;
	color.red = (int8_t)red;
	color.green = (int8_t)green;
	color.blue = (int8_t)blue;
	color.alpha = (int8_t)alpha;
}

//----------------------------------------------------------------------------------------------------
static bool process (CBitmap* bitmap, int32_t boxSize)
{
	CBitmapPixelAccess* accessor = CBitmapPixelAccess::create (bitmap, false);
	if (accessor)
	{
		int32_t x,y;
		int32_t width = (int32_t)bitmap->getWidth();
		int32_t height = (int32_t)bitmap->getHeight();
		CColor* nc = new CColor[boxSize];
		for (y = 0; y < height; y++)
		{
			accessor->setPosition (0, y);
			accessor->getColor (nc[0]);
			for (int32_t i = 1; i < boxSize; i++)
				nc[i] = kTransparentCColor;
			calculateBlurColor (nc[0], nc, boxSize);
			accessor->setColor (nc[0]);
			for (x = 1; x < width; x++)
			{
				accessor->setPosition (x, y);
				accessor->getColor (nc[0]);
				calculateBlurColor (nc[0], nc, boxSize);
				accessor->setColor (nc[0]);
			}
		}
		for (x = 0; x < width; x++)
		{
			accessor->setPosition (x, 0);
			accessor->getColor (nc[0]);
			for (int32_t i = 1; i < boxSize; i++)
				nc[i] = kTransparentCColor;
			calculateBlurColor (nc[0], nc, boxSize);
			accessor->setColor (nc[0]);
			for (y = 1; y < height; y++)
			{
				accessor->setPosition (x, y);
				accessor->getColor (nc[0]);
				calculateBlurColor (nc[0], nc, boxSize);
				accessor->setColor (nc[0]);
			}
		}
		delete [] nc;
		accessor->forget ();
		return true;
	}
	return false;
}

} // namespace

//----------------------------------------------------------------------------------------------------
IdStringPtr UIDialogController::kMsgDialogButton1Clicked = "UIDialogController::kMsgDialogButton1Clicked";
IdStringPtr UIDialogController::kMsgDialogButton2Clicked = "UIDialogController::kMsgDialogButton2Clicked";

//----------------------------------------------------------------------------------------------------
UIDialogController::UIDialogController (IController* baseController, CFrame* frame)
: DelegationController (baseController)
, frame (frame)
, dialogBackView (0)
{
}

//----------------------------------------------------------------------------------------------------
void UIDialogController::run (UTF8StringPtr _templateName, UTF8StringPtr _dialogTitle, UTF8StringPtr _button1, UTF8StringPtr _button2, IController* _dialogController, UIDescription* _description)
{
	collectOpenGLViews (frame);

	templateName = _templateName;
	dialogTitle = _dialogTitle;
	dialogButton1 = _button1;
	dialogButton2 = _button2 != 0 ? _button2 : "";
	dialogController = dynamic_cast<CBaseObject*> (_dialogController);
	dialogDescription = _description;
	CView* view = UIEditController::getEditorDescription ().createView ("dialog", this);
	if (view)
	{
		CRect size = view->getViewSize ();
		size.right += sizeDiff.x;
		size.bottom += sizeDiff.y;
		CRect frameSize = frame->getViewSize ();
		size.centerInside (frameSize);
		view->setViewSize (size);
		view->setMouseableArea (size);

		int32_t blurSize = 16;
		size.inset (-blurSize*2, -blurSize*2);
		size.offset (-5, -5);
		dialogBackView = new CViewContainer (size);
		dialogBackView->setTransparency (true);

		COffscreenContext* offscreen = COffscreenContext::create (frame, size.getWidth (), size.getHeight ());
		if (offscreen)
		{
			size.originize ();
			size.inset (blurSize*2, blurSize*2);
			offscreen->beginDraw ();
			offscreen->setFillColor (CColor (0, 0, 0, 50));
			offscreen->drawRect (size, kDrawFilled);
			offscreen->endDraw ();
			
			CBitmap* bitmap = offscreen->getBitmap ();
			BoxBlur::process (bitmap, blurSize);
			dialogBackView->setBackground (bitmap);
			
			offscreen->forget ();
		}
		
		frame->addView (dialogBackView);
		
		frame->setModalView (view);
		frame->registerKeyboardHook (this);
		if (button1)
			frame->setFocusView (button1);
		setOpenGLViewsVisible (false);
	}
	else
	{
		forget ();
	}
}

//----------------------------------------------------------------------------------------------------
void UIDialogController::valueChanged (CControl* control)
{
	if (control->getValue () == control->getMax ())
	{
		switch (control->getTag ())
		{
			case kButton1Tag:
			{
				dialogController->notify (this, kMsgDialogButton1Clicked);
				break;
			}
			case kButton2Tag:
			{
				dialogController->notify (this, kMsgDialogButton2Clicked);
				break;
			}
		}
		CView* modalView = frame->getModalView ();
		frame->setModalView (0);
		modalView->forget ();
		frame->unregisterKeyboardHook (this);
		if (dialogBackView)
			frame->removeView (dialogBackView);
		if (button1)
			button1->setListener (0);
		if (button2)
			button2->setListener (0);
		setOpenGLViewsVisible (true);
		forget ();
	}
}

//----------------------------------------------------------------------------------------------------
CControlListener* UIDialogController::getControlListener (UTF8StringPtr controlTagName)
{
	return this;
}

//----------------------------------------------------------------------------------------------------
CView* UIDialogController::verifyView (CView* view, const UIAttributes& attributes, IUIDescription* description)
{
	CControl* control = dynamic_cast<CControl*>(view);
	if (control)
	{
		if (control->getTag () == kButton1Tag)
		{
			CTextButton* button = dynamic_cast<CTextButton*>(control);
			if (button)
			{
				button1 = button;
				button->setTitle (dialogButton1.c_str ());
				layoutButtons ();
			}
		}
		else if (control->getTag () == kButton2Tag)
		{
			CTextButton* button = dynamic_cast<CTextButton*>(control);
			if (button)
			{
				button2 = button;
				if (dialogButton2.empty ())
				{
					button->setVisible (false);
				}
				else
				{
					button->setTitle (dialogButton2.c_str ());
				}
				layoutButtons ();
			}
		}
		else if (control->getTag () == kTitleTag)
		{
			CTextLabel* label = dynamic_cast<CTextLabel*>(control);
			if (label)
			{
				label->setText (dialogTitle.c_str ());
			}
		}
	}
	const std::string* name = attributes.getAttributeValue ("custom-view-name");
	if (name)
	{
		if (*name == "view")
		{
			CBaseObject* obj = dialogController;
			CView* subView = dialogDescription->createView (templateName.c_str (), dynamic_cast<IController*>(obj));
			if (subView)
			{
				subView->setAttribute (kCViewControllerAttribute, sizeof (CBaseObject*), &obj);
				sizeDiff.x = subView->getWidth () - view->getWidth ();
				sizeDiff.y = subView->getHeight () - view->getHeight ();
				CRect size = view->getViewSize ();
				size.setWidth (subView->getWidth ());
				size.setHeight (subView->getHeight ());
				view->setViewSize (size);
				view->setMouseableArea (size);
				CViewContainer* container = dynamic_cast<CViewContainer*> (view);
				if (container)
					container->addView (subView);
			}
		}
	}
	return view;
}

//----------------------------------------------------------------------------------------------------
void UIDialogController::layoutButtons ()
{
	if (button1 && button2)
	{
		CRect b1r1 = button1->getViewSize ();
		CRect b2r1 = button2->getViewSize ();
		CCoord margin = b1r1.left - b2r1.right;
		button1->sizeToFit ();
		button2->sizeToFit ();
		CRect b1r2 = button1->getViewSize ();
		CRect b2r2 = button2->getViewSize ();

		b1r2.offset (b1r1.getWidth () - b1r2.getWidth (), b1r1.getHeight () - b1r2.getHeight ());
		button1->setViewSize (b1r2);
		button1->setMouseableArea (b1r2);

		b2r2.offset (b2r1.getWidth () - b2r2.getWidth (), b2r1.getHeight () - b2r2.getHeight ());
		b2r2.offset ((b1r2.left - margin) - b2r2.right, 0); 
		button2->setViewSize (b2r2);
		button2->setMouseableArea (b2r2);
	}
}

//----------------------------------------------------------------------------------------------------
int32_t UIDialogController::onKeyDown (const VstKeyCode& code, CFrame* frame)
{
	CBaseObjectGuard guard (this);
	int32_t result = -1;
	CView* focusView = frame->getFocusView ();
	if (focusView)
		result = focusView->onKeyDown (const_cast<VstKeyCode&> (code));
	if (result == -1)
	{
		if (code.virt == VKEY_RETURN && code.modifier == 0)
		{
			button1->setValue (button1->getMax ());
			button1->valueChanged ();
			return 1;
		}
		if (code.virt == VKEY_ESCAPE && code.modifier == 0 && button2->isVisible ())
		{
			button2->setValue (button2->getMax ());
			button2->valueChanged ();
			return 1;
		}
	}
	return result;
}

//----------------------------------------------------------------------------------------------------
int32_t UIDialogController::onKeyUp (const VstKeyCode& code, CFrame* frame)
{
	CView* focusView = frame->getFocusView ();
	if (focusView)
		return focusView->onKeyUp (const_cast<VstKeyCode&> (code));
	return -1;
}

//----------------------------------------------------------------------------------------------------
void UIDialogController::collectOpenGLViews (CViewContainer* container)
{
#if VSTGUI_OPENGL_SUPPORT
	ViewIterator it (container);
	while (*it)
	{
		COpenGLView* openGLView = dynamic_cast<COpenGLView*>(*it);
		if (openGLView && openGLView->isVisible ())
			openglViews.push_back (openGLView);
		else
		{
			CViewContainer* childContainer = dynamic_cast<CViewContainer*>(*it);
			if (childContainer)
				collectOpenGLViews (childContainer);
		}
		it++;
	}
#endif
}

//----------------------------------------------------------------------------------------------------
void UIDialogController::setOpenGLViewsVisible (bool state)
{
	for (std::list<SharedPointer<COpenGLView> >::const_iterator it = openglViews.begin(); it != openglViews.end (); it++)
	{
		(*it)->setVisible (state);
	}
}

} // namespace

#endif // VSTGUI_LIVE_EDITING