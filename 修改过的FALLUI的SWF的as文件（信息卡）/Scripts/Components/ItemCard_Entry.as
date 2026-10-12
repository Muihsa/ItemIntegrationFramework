package Components
{
   import M8r.Data.GameItemDataExtractor;
   import M8r.Helper.Colors;
   import M8r.Helper.ColorsRec;
   import M8r.Helper.FontLoader;
   import M8r.Helper.Translation;
   import M8r.Helper.TryHard;
   import M8r.Model.FallUIConfig;
   import M8r.Mods;
   import Shared.GlobalFunc;
   import flash.display.DisplayObject;
   import flash.display.Graphics;
   import flash.display.MovieClip;
   import flash.display.Shape;
   import flash.display.Sprite;
   import flash.geom.ColorTransform;
   import flash.system.ApplicationDomain;
   import flash.text.TextField;
   import flash.text.TextFormat;
   import flash.utils.getQualifiedClassName;
   import scaleform.gfx.Extensions;
   import scaleform.gfx.TextFieldEx;
   
   public class ItemCard_Entry extends MovieClip
   {
      
      protected const modDiffWidth:Number = 50;
      
      protected const modStyleEntityReduceHeight:Number = 5;
      
      protected const modStyleTfReduceHeight:Number = 4;
      
      protected const modStyleTfSpaceBetween:Number = 6;
      
      public var Label_tf:TextField;
      
      public var Value_tf:TextField;
      
      public var Comparison_mc:MovieClip;
      
      public var border_mc:MovieClip;
      
      public var dif_tf:TextField;
      
      public var ammoicon_mc:MovieClip = null;
      
      public var background:MovieClip = null;
      
      public var itemCardReference:ItemCard = null;
      
      public function ItemCard_Entry()
      {
         var _loc5_:TextFormat = null;
         super();
         Extensions.enabled = true;
         var _loc1_:TextField = this.Label_tf;
         var _loc2_:FontLoader = FontLoader.instance;
         if(_loc1_)
         {
            TextFieldEx.setTextAutoSize(_loc1_,TextFieldEx.TEXTAUTOSZ_SHRINK);
            _loc1_.height -= this.modStyleEntityReduceHeight;
            _loc1_.height -= this.modStyleTfReduceHeight / 2;
            _loc1_.x += 2;
            _loc2_.applyFont(_loc1_);
            if(getQualifiedClassName(this) === "ItemCard_AmmoEntry")
            {
               _loc1_.width -= this.modDiffWidth - 10;
            }
         }
         var _loc3_:TextField = this.Value_tf;
         if(_loc3_)
         {
            TextFieldEx.setTextAutoSize(_loc3_,TextFieldEx.TEXTAUTOSZ_SHRINK);
            _loc3_.height -= this.modStyleEntityReduceHeight;
            _loc3_.height -= this.modStyleTfReduceHeight / 2;
            _loc3_.x += 2;
            _loc2_.applyFont(_loc3_);
         }
         var _loc4_:TextField = this.dif_tf;
         if(!_loc4_)
         {
            if(_loc3_)
            {
               if(this is ItemCard_StandardEntry || this is Components.ItemCard_MultiEntry_Value)
               {
                  this.addChild(_loc4_ = new TextField());
                  this.dif_tf = _loc4_;
                  _loc4_.y = _loc3_.y;
                  _loc4_.height = _loc3_.height;
                  _loc4_.width = this.modDiffWidth;
                  _loc5_ = _loc3_.defaultTextFormat;
                  _loc5_.align = "left";
                  _loc2_.applyFont(_loc4_,_loc5_,0.85);
                  if(_loc1_)
                  {
                     _loc1_.width -= _loc4_.width - 10;
                  }
                  _loc3_.x -= _loc4_.width - 10;
                  _loc3_.width -= 10 + this.modStyleTfSpaceBetween;
                  _loc4_.x = _loc3_.x + _loc3_.width + this.modStyleTfSpaceBetween;
                  TextFieldEx.setTextAutoSize(_loc4_,TextFieldEx.TEXTAUTOSZ_SHRINK);
               }
            }
         }
         if(this.getChildAt(0))
         {
            this.background = this.getChildAt(0) as MovieClip;
            this.getChildAt(0).height = this.getChildAt(0).height - this.modStyleEntityReduceHeight;
         }
         else
         {
            this.background = new MovieClip();
         }
      }
      
      public static function ShouldShowDifference(param1:Object) : Boolean
      {
         var _loc2_:int = int(param1.precision != undefined ? param1.precision : 0);
         var _loc3_:Number = 1;
         while(_loc2_--)
         {
            _loc3_ /= 10;
         }
         return Math.abs(param1.difference) >= _loc3_;
      }
      
      public function ShouldShowDifference2(param1:Object) : Boolean
      {
         var _loc2_:int = int(param1.precision != undefined ? param1.precision : 0);
         var _loc3_:Number = 1;
         while(_loc2_--)
         {
            _loc3_ /= 10;
         }
         return Math.abs(this.modGetDiffToShow(param1)) >= _loc3_;
      }
      
      private function modGetDiffToShow(param1:Object) : Number
      {
         var _loc3_:* = undefined;
         var _loc4_:Number = Number(NaN);
         if(!this.itemCardReference)
         {
            return 0;
         }
         var _loc2_:ItemCard = this.itemCardReference.modCompareToItem;
         if(_loc2_)
         {
            if(param1.text in _loc2_.modMyData)
            {
               _loc3_ = _loc2_.modMyData[param1.text];
               _loc4_ = 0;
               if(param1.damageType == undefined || param1.text === "$dmg")
               {
                  _loc4_ = Number(_loc3_ || 0);
               }
               else if(typeof _loc3_ === "object")
               {
                  _loc4_ = Number(_loc3_[param1.damageType] || 0);
               }
               return param1.value - _loc4_;
            }
         }
         else if(param1.difference != undefined)
         {
            return param1.difference;
         }
         return 0;
      }
      
      public function PopulateEntry(param1:Object) : *
      {
         TryHard.tryFunction(this.PopulateEntryTried,"PopulateEntry",param1);
      }
      
      private function PopulateEntryTried(param1:Object) : *
      {
         var _loc9_:DisplayObject = null;
         var _loc12_:MovieClip = null;
         var _loc13_:Shape = null;
         var _loc14_:Graphics = null;
         var _loc15_:TextFormat = null;
         var _loc16_:String = null;
         var _loc2_:Number = 0;
         var _loc3_:String = "";
         var _loc4_:MovieClip = this.border_mc;
         var _loc5_:FallUIConfig = Mods.config;
         var _loc6_:TextField = this.Label_tf;
         if(_loc4_)
         {
            if(_loc5_.Itemcard.bShowDiffRait || _loc5_.Itemcard.bShowTimed)
            {
               _loc4_.x -= 50;
               _loc4_.width += 100;
               if(_loc6_)
               {
                  _loc6_.x -= 50;
                  _loc6_.width += 50;
               }
               if(this.ammoicon_mc)
               {
                  this.ammoicon_mc.x -= 50;
               }
            }
         }
         var _loc7_:int = Mods.targetEnvironment;
         var _loc8_:Boolean = _loc7_ === Mods.ENVIRONMENT_PIPBOY_MENU;
         if(_loc7_ === Mods.ENVIRONMENT_EXAMINE_MENU || _loc8_ || _loc7_ === Mods.ENVIRONMENT_CONTAINER)
         {
            if(getQualifiedClassName(this) === "ItemCard_AmmoEntry")
            {
               if(!this.ammoicon_mc)
               {
                  _loc9_ = this.getChildAt(3);
                  if(_loc9_)
                  {
                     if(_loc9_.name.indexOf("instance") === 0)
                     {
                        _loc9_.y -= 3;
                        if(Mods.configLoaded)
                        {
                           if(_loc5_.stdColor !== 16777215)
                           {
                              if(_loc8_ && _loc5_.Interface.iPipboyDualColoring || _loc7_ === Mods.ENVIRONMENT_CONTAINER && _loc5_.Interface.iContainerDualColorColorize)
                              {
                                 ColorsRec.colorize(_loc9_,false,true);
                              }
                              else
                              {
                                 ColorsRec.colorize(_loc9_);
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
         if(Mods.configLoaded)
         {
            if(_loc5_.stdColor !== 16777215)
            {
               if(this.ammoicon_mc)
               {
                  ColorsRec.colorize(this.ammoicon_mc);
               }
               ColorsRec.colorize(this["Icon_mc"]);
            }
         }
         var oldBg:DisplayObject = this.getChildByName("iif_manual_bg");
         if(oldBg)
         {
            this.removeChild(oldBg);
         }
         var oldIcon1:DisplayObject = this.getChildByName("iif_icon_1");
         if(oldIcon1)
         {
            this.removeChild(oldIcon1);
         }
         var oldIcon2:DisplayObject = this.getChildByName("iif_icon_2");
         if(oldIcon2)
         {
            this.removeChild(oldIcon2);
         }
         var oldLine:DisplayObject = this.getChildByName("lines");
         if(oldLine)
         {
            this.removeChild(oldLine);
         }
         if(this.background)
         {
            if(param1.hasBackground === true && !_loc8_)
            {
               this.background.alpha = 0;
               var myBg:Shape = new Shape();
               myBg.name = "iif_manual_bg";
               var customBgColor:uint = param1.backgroundColor != undefined && param1.backgroundColor != 0 ? uint(param1.backgroundColor) : 6513507;
               myBg.graphics.beginFill(customBgColor,1);
               myBg.graphics.drawRect(0,0,this.background.width - 2.5,this.background.height);
               myBg.graphics.endFill();
               myBg.x = this.background.x;
               myBg.y = this.background.y;
               this.addChildAt(myBg,0);
            }
            else
            {
               this.background.alpha = 1;
               this.background.transform.colorTransform = new ColorTransform();
            }
         }
         if(param1.displayType == 2)
         {
            this.modPopulateDualEntry(param1,_loc5_);
            return;
         }
         if(param1.displayType == 1)
         {
            this.modPopulateBarEntry(param1,_loc5_);
            return;
         }
         if(_loc6_)
         {
            _loc6_.textColor = this.ammoicon_mc ? _loc5_.stdColor : _loc5_.stdColor2;
            var rawLabel:String = param1.label != undefined && param1.label != "" ? param1.label : param1.text;
            GlobalFunc.SetText(_loc6_,Translation.translate(rawLabel),false);
         }
         var _loc10_:TextField = this.Value_tf;
         var _loc11_:TextField = this.dif_tf;
         if(_loc10_)
         {
            if(param1.valueStandard === true || param1.state == "standard")
            {
               _loc10_.textColor = uint(_loc5_.stdColor2);
            }
            else if(param1.valueBad === true || param1.state == "bad")
            {
               _loc10_.textColor = uint(Mods.styleDifferenceColorBad);
            }
            else if(param1.valueGood === true || param1.state == "good")
            {
               _loc10_.textColor = uint(Mods.styleDifferenceColorGood);
            }
            else if(param1.valueColor != undefined && uint(param1.valueColor) != 0)
            {
               _loc10_.textColor = uint(param1.valueColor);
            }
            if(this.itemCardReference)
            {
               if(_loc5_.Itemcard.bShowDiffRait)
               {
                  if(_loc11_)
                  {
                     if(this.itemCardReference.hideDiff || param1.hideDifference === true)
                     {
                        _loc11_.text = "";
                     }
                     else
                     {
                        _loc2_ = this.modGetDiffToShow(param1);
                        if(_loc2_)
                        {
                           if(param1.text == "$wt" && this.itemCardReference.modCompareToItem == null)
                           {
                              _loc2_ = -1 * _loc2_;
                           }
                           _loc3_ = _loc2_.toString();
                           if(_loc3_.indexOf(".") != -1)
                           {
                              _loc3_ = _loc3_.substring(0,Math.min(_loc3_.indexOf(".") + 2,_loc3_.length));
                           }
                           if(_loc2_ > 0)
                           {
                              _loc3_ = "+" + _loc3_;
                           }
                           else if(_loc2_ == 0)
                           {
                              _loc3_ = "";
                           }
                           if(_loc3_ != "" && param1.suffix != undefined)
                           {
                              _loc3_ += param1.suffix;
                           }
                           GlobalFunc.SetText(_loc11_,_loc3_,false,false,false,true);
                           if(!Mods.configLoaded || !_loc8_ && _loc5_.Itemcard.bContainerColored || _loc8_ && _loc5_.Itemcard.bPipboyColored)
                           {
                              if(param1.invertDiffColor === true)
                              {
                                 var isBad:Boolean = _loc2_ > 0;
                              }
                              else if(param1.invertDiffColor === false)
                              {
                                 isBad = _loc2_ < 0;
                              }
                              else
                              {
                                 isBad = _loc2_ < 0 === (param1.text !== "$wt");
                              }
                              if(isBad)
                              {
                                 _loc11_.textColor = Mods.styleDifferenceColorBad;
                              }
                              else
                              {
                                 _loc11_.textColor = Mods.styleDifferenceColorGood;
                              }
                              Colors.decolorColoredChild(_loc11_,true);
                           }
                        }
                     }
                  }
               }
            }
            if(param1.valueText != undefined && param1.valueText != "")
            {
               GlobalFunc.SetText(_loc10_,Translation.translate(param1.valueText),false);
            }
            else
            {
               GlobalFunc.SetText(_loc10_,GameItemDataExtractor.formatEntryText(param1),false);
            }
            if(this.height)
            {
               _loc12_ = this.background;
               if(_loc12_)
               {
                  if(this.itemCardReference)
                  {
                     _loc13_ = new Shape();
                     _loc13_.name = "lines";
                     _loc14_ = _loc13_.graphics;
                     _loc14_.lineStyle(2,this.itemCardReference.iStyleBgColor,1);
                     if(_loc10_)
                     {
                        _loc14_.moveTo(_loc10_.x - 3,_loc12_.y);
                        _loc14_.lineTo(_loc10_.x - 3,_loc12_.height + _loc12_.y);
                     }
                     if(_loc11_)
                     {
                        _loc14_.moveTo(_loc11_.x - this.modStyleTfSpaceBetween / 2,_loc12_.y);
                        _loc14_.lineTo(_loc11_.x - this.modStyleTfSpaceBetween / 2,_loc12_.height + _loc12_.y);
                     }
                     if(getQualifiedClassName(this) === "ItemCard_AmmoEntry")
                     {
                        _loc13_.x = -this.modDiffWidth + 10;
                     }
                     this.addChild(_loc13_);
                  }
               }
            }
         }
         if(this.Comparison_mc)
         {
            if(this.ShouldShowDifference2(param1))
            {
               if(!_loc5_.Itemcard.bShowDiffRait)
               {
                  if(_loc11_)
                  {
                     _loc15_ = _loc11_.defaultTextFormat;
                     _loc15_.align = "center";
                     _loc11_.defaultTextFormat = _loc15_;
                     _loc16_ = "";
                     if(this.itemCardReference && this.itemCardReference.modCompareToItem)
                     {
                        _loc16_ = "";
                     }
                     else
                     {
                        switch(param1.diffRating)
                        {
                           case -3:
                              _loc16_ = "- - -";
                              break;
                           case -2:
                              _loc16_ = "- -";
                              break;
                           case -1:
                              _loc16_ = "-";
                              break;
                           case 1:
                              _loc16_ = "+";
                              break;
                           case 2:
                              _loc16_ = "+ +";
                              break;
                           case 3:
                              _loc16_ = "+ + +";
                              break;
                           default:
                              _loc16_ = "";
                        }
                     }
                     _loc11_.text = _loc16_;
                  }
               }
            }
         }
      }
      
      private function modPopulateDualEntry(param1:Object, param2:FallUIConfig) : void
      {
         var rawLabel:String;
         var _loc10_:TextField;
         var _loc11_:TextField;
         var lineX:Number;
         var rightEdge:Number;
         var startX:Number;
         var totalRightW:Number;
         var reqPad:Number;
         var quarterW:Number;
         var rawIcon1:String;
         var val1Str:String;
         var val1Bad:Boolean;
         var val1Good:Boolean;
         var val1Standard:Boolean;
         var icon1IsText:Boolean;
         var tagStr1:String;
         var rawIcon2:String;
         var val2Str:String;
         var val2Bad:Boolean;
         var val2Good:Boolean;
         var val2Standard:Boolean;
         var icon2IsText:Boolean;
         var tagStr2:String;
         var hasLeft:Boolean;
         var hasRight:Boolean;
         var color1:uint;
         var color2:uint;
         var tf1:TextFormat;
         var tf2:TextFormat;
         var skillIconSprite:Sprite;
         var strIconSprite:Sprite;
         var IconLibCls:Object;
         var align1:String;
         var align2:String;
         var hasIcon1:Boolean;
         var hasIcon2:Boolean;
         var rightText1:String;
         var rightText2:String;
         var tfFix1:TextFormat;
         var tfFix2:TextFormat;
         var txtW1:Number;
         var txtW2:Number;
         var txtLeftX2:Number;
         var ct1:ColorTransform;
         var ct2:ColorTransform;
         var reqLines:Shape;
         var _loc6_:TextField = this.Label_tf;
         if(_loc6_)
         {
            _loc6_.textColor = param1.highlightLabel === true ? param2.stdColor : (this.ammoicon_mc ? param2.stdColor : param2.stdColor2);
            rawLabel = param1.label != undefined && param1.label != "" ? param1.label : param1.text;
            GlobalFunc.SetText(_loc6_,Translation.translate(rawLabel),false);
         }
         _loc10_ = this.Value_tf;
         _loc11_ = this.dif_tf;
         if(_loc10_)
         {
            _loc10_.visible = false;
            _loc10_.text = "";
         }
         if(_loc11_)
         {
            _loc11_.visible = false;
            _loc11_.text = "";
         }
         if(this.Comparison_mc)
         {
            this.Comparison_mc.visible = false;
         }
         lineX = 0;
         if(_loc10_ && _loc11_)
         {
            rightEdge = _loc11_.x + _loc11_.width;
            startX = _loc10_.x;
            lineX = startX;
            totalRightW = rightEdge - startX;
            reqPad = 4;
            quarterW = Math.round(totalRightW / 2);
            rawIcon1 = param1.icon1 != undefined ? param1.icon1 : "";
            val1Str = param1.val1 != undefined ? param1.val1 : "";
            val1Bad = param1.val1Bad === true;
            val1Good = param1.val1Good === true;
            val1Standard = param1.val1Standard === true;
            icon1IsText = param1.icon1IsText === true;
            tagStr1 = rawIcon1.replace(/^\[|\]$/g,"");
            rawIcon2 = param1.icon2 != undefined ? param1.icon2 : "";
            val2Str = param1.val2 != undefined ? param1.val2 : "";
            val2Bad = param1.val2Bad === true;
            val2Good = param1.val2Good === true;
            val2Standard = param1.val2Standard === true;
            icon2IsText = param1.icon2IsText === true;
            tagStr2 = rawIcon2.replace(/^\[|\]$/g,"");
            hasLeft = rawIcon1 != "" || val1Str != "";
            hasRight = rawIcon2 != "" || val2Str != "";
            color1 = uint(param2.stdColor);
            if(val1Bad)
            {
               color1 = uint(Mods.styleDifferenceColorBad);
            }
            else if(val1Good)
            {
               color1 = uint(Mods.styleDifferenceColorGood);
            }
            else if(val1Standard)
            {
               color1 = uint(param2.stdColor2);
            }
            color2 = uint(param2.stdColor);
            if(val2Bad)
            {
               color2 = uint(Mods.styleDifferenceColorBad);
            }
            else if(val2Good)
            {
               color2 = uint(Mods.styleDifferenceColorGood);
            }
            else if(val2Standard)
            {
               color2 = uint(param2.stdColor2);
            }
            tf1 = null;
            tf2 = null;
            if(hasLeft && hasRight)
            {
               _loc10_.visible = true;
               _loc11_.visible = true;
               _loc10_.x = startX + reqPad;
               _loc10_.width = quarterW - reqPad;
               _loc11_.x = startX + quarterW;
               _loc11_.width = totalRightW - quarterW;
               tf1 = _loc10_.defaultTextFormat;
               tf1.align = "left";
               _loc10_.defaultTextFormat = tf1;
               tf2 = _loc11_.defaultTextFormat;
               tf2.align = "right";
               _loc11_.defaultTextFormat = tf2;
            }
            else if(hasLeft && !hasRight)
            {
               _loc10_.visible = true;
               _loc11_.visible = false;
               _loc10_.x = startX;
               _loc10_.width = totalRightW;
               tf1 = _loc10_.defaultTextFormat;
               tf1.align = "right";
               _loc10_.defaultTextFormat = tf1;
            }
            else if(!hasLeft && hasRight)
            {
               _loc10_.visible = false;
               _loc11_.visible = true;
               _loc11_.x = startX;
               _loc11_.width = totalRightW;
               tf2 = _loc11_.defaultTextFormat;
               tf2.align = "right";
               _loc11_.defaultTextFormat = tf2;
            }
            skillIconSprite = null;
            strIconSprite = null;
            IconLibCls = null;
            try
            {
               IconLibCls = ApplicationDomain.currentDomain.getDefinition("M8r.Service.IconLibrary");
               if(IconLibCls && IconLibCls.instance)
               {
                  if(!icon1IsText && tagStr1 != "")
                  {
                     skillIconSprite = IconLibCls.instance.makeTagIcon(tagStr1,18) as Sprite;
                  }
                  if(!icon2IsText && tagStr2 != "")
                  {
                     strIconSprite = IconLibCls.instance.makeTagIcon(tagStr2,18) as Sprite;
                  }
               }
            }
            catch(e:Error)
            {
            }
            align1 = param1.align1 != undefined && param1.align1 != "" ? param1.align1 : "right";
            align2 = param1.align2 != undefined && param1.align2 != "" ? param1.align2 : "right";
            hasIcon1 = false;
            hasIcon2 = false;
            rightText1 = null;
            rightText2 = null;
            tfFix1 = null;
            tfFix2 = null;
            txtW1 = 0;
            txtW2 = 0;
            txtLeftX2 = 0;
            ct1 = null;
            ct2 = null;
            if(hasLeft)
            {
               hasIcon1 = skillIconSprite != null;
               rightText1 = skillIconSprite ? val1Str : (rawIcon1 != "" ? rawIcon1 + "  " : "") + val1Str;
               GlobalFunc.SetText(_loc10_,rightText1,false);
               _loc10_.textColor = color1;
               tfFix1 = _loc10_.defaultTextFormat;
               tfFix1.align = "left";
               _loc10_.defaultTextFormat = tfFix1;
               _loc10_.setTextFormat(tfFix1);
               if(!hasRight)
               {
                  txtW1 = _loc10_.textWidth;
                  if(txtW1 < 5)
                  {
                     txtW1 = rightText1.length * 8;
                  }
                  _loc10_.width = txtW1 + 5;
                  if(align1 == "left")
                  {
                     _loc10_.x = startX + (hasIcon1 ? 22 : 0);
                  }
                  else if(align1 == "center")
                  {
                     _loc10_.x = startX + (totalRightW - _loc10_.width) / 2 + (hasIcon1 ? 11 : 0);
                  }
                  else
                  {
                     _loc10_.x = rightEdge - _loc10_.width;
                  }
               }
               if(skillIconSprite)
               {
                  skillIconSprite.name = "iif_icon_1";
                  if(hasRight)
                  {
                     skillIconSprite.x = startX + reqPad;
                     _loc10_.x += 22;
                     _loc10_.width -= 22;
                  }
                  else
                  {
                     skillIconSprite.x = _loc10_.x - 22;
                  }
                  skillIconSprite.y = Math.round(_loc10_.y + (_loc10_.height - 18) / 2);
                  ct1 = new ColorTransform();
                  ct1.color = color1;
                  skillIconSprite.transform.colorTransform = ct1;
                  this.addChild(skillIconSprite);
               }
            }
            if(hasRight)
            {
               hasIcon2 = strIconSprite != null;
               rightText2 = strIconSprite ? val2Str : (rawIcon2 != "" ? rawIcon2 + "  " : "") + val2Str;
               GlobalFunc.SetText(_loc11_,rightText2,false);
               _loc11_.textColor = color2;
               tfFix2 = _loc11_.defaultTextFormat;
               tfFix2.align = hasLeft ? "right" : "left";
               _loc11_.defaultTextFormat = tfFix2;
               _loc11_.setTextFormat(tfFix2);
               if(!hasLeft)
               {
                  txtW2 = _loc11_.textWidth;
                  if(txtW2 < 5)
                  {
                     txtW2 = rightText2.length * 8;
                  }
                  _loc11_.width = txtW2 + 5;
                  if(align2 == "left")
                  {
                     _loc11_.x = startX + (hasIcon2 ? 22 : 0);
                  }
                  else if(align2 == "center")
                  {
                     _loc11_.x = startX + (totalRightW - _loc11_.width) / 2 + (hasIcon2 ? 11 : 0);
                  }
                  else
                  {
                     _loc11_.x = rightEdge - _loc11_.width;
                  }
               }
               if(strIconSprite)
               {
                  strIconSprite.name = "iif_icon_2";
                  if(hasLeft)
                  {
                     txtW2 = _loc11_.textWidth;
                     if(txtW2 < 5)
                     {
                        txtW2 = rightText2.length * 8;
                     }
                     txtLeftX2 = _loc11_.x + _loc11_.width - txtW2 - 4;
                     strIconSprite.x = txtLeftX2 - 24;
                  }
                  else
                  {
                     strIconSprite.x = _loc11_.x - 22;
                  }
                  strIconSprite.y = Math.round(_loc11_.y + (_loc11_.height - 18) / 2);
                  ct2 = new ColorTransform();
                  ct2.color = color2;
                  strIconSprite.transform.colorTransform = ct2;
                  this.addChild(strIconSprite);
               }
            }
         }
         if(this.height && this.background && _loc10_)
         {
            reqLines = new Shape();
            reqLines.name = "lines";
            reqLines.graphics.lineStyle(2,this.itemCardReference ? this.itemCardReference.iStyleBgColor : 16777215,1);
            reqLines.graphics.moveTo(lineX - 3,this.background.y);
            reqLines.graphics.lineTo(lineX - 3,this.background.height + this.background.y);
            this.addChild(reqLines);
         }
      }
      
      private function modPopulateBarEntry(param1:Object, param2:FallUIConfig) : void
      {
         var _loc6_:TextField = this.Label_tf;
         if(_loc6_)
         {
            _loc6_.textColor = param1.highlightLabel === true ? param2.stdColor : (this.ammoicon_mc ? param2.stdColor : param2.stdColor2);
            var rawLabel:String = param1.label != undefined && param1.label != "" ? param1.label : param1.text;
            GlobalFunc.SetText(_loc6_,Translation.translate(rawLabel),false);
         }
         var _loc10_:TextField = this.Value_tf;
         var _loc11_:TextField = this.dif_tf;
         if(_loc11_)
         {
            _loc11_.visible = false;
            _loc11_.text = "";
         }
         if(this.Comparison_mc)
         {
            this.Comparison_mc.visible = false;
         }
         var showBar:Boolean = param1.showBar != undefined ? param1.showBar === true : true;
         var showValue:Boolean = param1.showValue === true;
         var valueAlign:String = param1.valueAlign != undefined && param1.valueAlign != "" ? param1.valueAlign : "right";
         var barBgShape:Shape = this.getChildByName("iif_bar_bg") as Shape;
         var barFillShape:Shape = this.getChildByName("iif_bar_fill") as Shape;
         var barShieldShape:Shape = this.getChildByName("iif_bar_shield") as Shape;
         var barThresholdShape:Shape = this.getChildByName("iif_bar_threshold") as Shape;
         if(!barBgShape)
         {
            barBgShape = new Shape();
            barBgShape.name = "iif_bar_bg";
            this.addChild(barBgShape);
            barFillShape = new Shape();
            barFillShape.name = "iif_bar_fill";
            this.addChild(barFillShape);
            barShieldShape = new Shape();
            barShieldShape.name = "iif_bar_shield";
            this.addChild(barShieldShape);
            barThresholdShape = new Shape();
            barThresholdShape.name = "iif_bar_threshold";
            this.addChild(barThresholdShape);
         }
         barBgShape.alpha = 1;
         barFillShape.alpha = 1;
         barShieldShape.alpha = 1;
         barThresholdShape.alpha = 1;
         barBgShape.visible = showBar;
         barFillShape.visible = showBar;
         barShieldShape.visible = showBar;
         barThresholdShape.visible = showBar;
         var totalW:Number = 100;
         var startX:Number = 0;
         var rightEdge:Number = 100;
         if(_loc10_)
         {
            startX = _loc10_.x;
            totalW = _loc10_.width;
            rightEdge = startX + totalW;
            if(_loc11_)
            {
               totalW = _loc11_.x + _loc11_.width - _loc10_.x;
               rightEdge = _loc11_.x + _loc11_.width;
            }
            _loc10_.visible = showValue;
            _loc10_.alpha = showValue ? 1 : 0;
            if(showValue)
            {
               if(param1.valueColor != undefined && uint(param1.valueColor) != 0)
               {
                  _loc10_.textColor = uint(param1.valueColor);
               }
               else if(param1.valBad === true || param1.state == "bad")
               {
                  _loc10_.textColor = uint(Mods.styleDifferenceColorBad);
               }
               else if(param1.valGood === true || param1.state == "good")
               {
                  _loc10_.textColor = uint(Mods.styleDifferenceColorGood);
               }
               else if(param1.valueStandard === true || param1.state == "standard")
               {
                  _loc10_.textColor = uint(param2.stdColor2);
               }
               else
               {
                  _loc10_.textColor = uint(param2.stdColor);
               }
               if(param1.valueText != undefined && param1.valueText != "")
               {
                  GlobalFunc.SetText(_loc10_,Translation.translate(param1.valueText),false);
               }
               else if(param1.value != undefined)
               {
                  GlobalFunc.SetText(_loc10_,Translation.translate(param1.value),false);
               }
               var tfFix:TextFormat = _loc10_.defaultTextFormat;
               tfFix.align = "left";
               _loc10_.defaultTextFormat = tfFix;
               _loc10_.setTextFormat(tfFix);
               var txtW:Number = _loc10_.textWidth;
               if(txtW < 5)
               {
                  txtW = _loc10_.text.length * 8;
               }
               _loc10_.width = txtW + 5;
               if(valueAlign == "left")
               {
                  _loc10_.x = startX;
               }
               else if(valueAlign == "center")
               {
                  _loc10_.x = startX + (totalW - _loc10_.width) / 2;
               }
               else
               {
                  _loc10_.x = rightEdge - _loc10_.width;
               }
               this.setChildIndex(_loc10_,this.numChildren - 1);
            }
         }
         var bgW:Number = totalW - 4 * 2;
         if(_loc10_)
         {
            barBgShape.x = startX + 4;
            barBgShape.y = Math.round(_loc10_.y + _loc10_.height / 2 - 20 / 2);
         }
         barFillShape.x = barBgShape.x + 5;
         barFillShape.y = barBgShape.y + 4;
         barShieldShape.x = barFillShape.x;
         barShieldShape.y = barFillShape.y;
         barThresholdShape.x = barFillShape.x;
         barThresholdShape.y = barFillShape.y;
         if(param1.fillPct != undefined)
         {
            var basePct:Number = Number(param1.fillPct);
            var shieldPct:Number = param1.shieldPct != undefined ? Number(param1.shieldPct) : 0;
            var shieldDrawPct:Number = Math.min(1,shieldPct);
         }
         else
         {
            var pct:Number = 1;
            if(_loc10_)
            {
               pct = parseFloat(_loc10_.text) / 100;
            }
            if(isNaN(pct) || pct < 0)
            {
               pct = 0;
            }
            basePct = Math.min(1,pct);
            shieldPct = Math.max(0,pct - 1);
            shieldDrawPct = Math.min(1,shieldPct / 1);
         }
         var passedColor:uint = param1.fillColor != undefined ? uint(param1.fillColor) : 0;
         var uiColor:uint = passedColor != 0 && passedColor != 16777215 ? passedColor : uint(Mods.config.stdColor);
         var origR:uint = uint(uiColor >> 16 & 0xFF);
         var origG:uint = uint(uiColor >> 8 & 0xFF);
         var origB:uint = uint(uiColor & 0xFF);
         if(origR + origG + origB > 650)
         {
            var shieldColor:uint = uint(origR * 0.65 << 16 | origG * 0.65 << 8 | origB * 0.65);
         }
         else
         {
            var br:uint = origR + (255 - origR) * 0.7;
            var bg:uint = origG + (255 - origG) * 0.7;
            var bb:uint = origB + (255 - origB) * 0.7;
            shieldColor = uint(br << 16 | bg << 8 | bb);
         }
         var bgDarkR:uint = uint(Math.max(1,Math.round(origR * 0.18)));
         var bgDarkG:uint = uint(Math.max(1,Math.round(origG * 0.18)));
         var bgDarkB:uint = uint(Math.max(1,Math.round(origB * 0.18)));
         var bgColor:uint = uint(bgDarkR << 16 | bgDarkG << 8 | bgDarkB);
         barBgShape.graphics.clear();
         if(showBar)
         {
            barBgShape.graphics.lineStyle(2,uiColor,0.8);
            barBgShape.graphics.beginFill(bgColor,1);
            barBgShape.graphics.drawRect(0,0,bgW,20);
            barBgShape.graphics.endFill();
         }
         var maxFillW:Number = bgW - 5 * 2;
         var fillH:Number = 20 - 4 * 2;
         var nubH:Number = fillH * 0.5;
         var nubY:Number = (fillH - nubH) / 2;
         barFillShape.graphics.clear();
         barThresholdShape.graphics.clear();
         var fillWidth:Number = maxFillW * basePct;
         var notchPct1:Number = param1.thresholdPct != undefined ? Number(param1.thresholdPct) : -1;
         var notchPct2:Number = param1.thresholdPct2 != undefined ? Number(param1.thresholdPct2) : -1;
         var drawNotch:Boolean = shieldDrawPct <= 0;
         if(showBar && fillWidth > 0)
         {
            barFillShape.graphics.beginFill(uiColor,0.8);
            if(fillWidth <= 1)
            {
               barFillShape.graphics.drawRect(0,nubY,fillWidth,nubH);
               barFillShape.graphics.endFill();
            }
            else
            {
               barFillShape.graphics.drawRect(0,nubY,1,nubH);
               barFillShape.graphics.drawRect(1,0,fillWidth - 1,fillH);
               barFillShape.graphics.endFill();
               if(drawNotch && (notchPct1 >= 0 || notchPct2 >= 0))
               {
                  var markList:Array = [];
                  if(notchPct1 >= 0 && notchPct1 <= 1)
                  {
                     var mp1:Number = maxFillW * notchPct1;
                     if(mp1 > 1 && mp1 < maxFillW)
                     {
                        markList.push(mp1);
                     }
                  }
                  if(notchPct2 >= 0 && notchPct2 <= 1)
                  {
                     var mp2:Number = maxFillW * notchPct2;
                     if(mp2 > 1 && mp2 < maxFillW)
                     {
                        var isDup2:Boolean = false;
                        var di2:int = 0;
                        while(di2 < markList.length)
                        {
                           if(Math.abs(markList[di2] - mp2) < 5)
                           {
                              isDup2 = true;
                              break;
                           }
                           di2++;
                        }
                        if(!isDup2)
                        {
                           markList.push(mp2);
                        }
                     }
                  }
                  if(markList.length > 1 && markList[0] > markList[1])
                  {
                     var mtmp:Number = Number(markList[0]);
                     markList[0] = markList[1];
                     markList[1] = mtmp;
                  }
                  var triH:Number = Math.max(1,Math.round(fillH / 3));
                  var triBw:Number = Math.max(1,Math.round(fillH * 0.25));
                  var triColor:uint = bgColor;
                  barThresholdShape.graphics.beginFill(triColor,1);
                  var mi:int = 0;
                  while(mi < markList.length)
                  {
                     var mx:Number = Number(markList[mi]);
                     barThresholdShape.graphics.moveTo(mx - triBw,0);
                     barThresholdShape.graphics.lineTo(mx,triH);
                     barThresholdShape.graphics.lineTo(mx + triBw,0);
                     barThresholdShape.graphics.lineTo(mx - triBw,0);
                     barThresholdShape.graphics.moveTo(mx - triBw,fillH);
                     barThresholdShape.graphics.lineTo(mx,fillH - triH);
                     barThresholdShape.graphics.lineTo(mx + triBw,fillH);
                     barThresholdShape.graphics.lineTo(mx - triBw,fillH);
                     mi++;
                  }
                  barThresholdShape.graphics.endFill();
               }
            }
         }
         barShieldShape.graphics.clear();
         var shieldFillWidth:Number = maxFillW * shieldDrawPct;
         if(showBar && shieldFillWidth > 0)
         {
            var shieldH:Number = fillH * 0.33;
            var shieldY:Number = (fillH - shieldH) / 2;
            var shieldX:Number = maxFillW - shieldFillWidth;
            barShieldShape.graphics.beginFill(shieldColor,1);
            barShieldShape.graphics.drawRect(shieldX,shieldY,shieldFillWidth,shieldH);
            barShieldShape.graphics.endFill();
         }
         var tL:Shape = this.getChildByName("lines") as Shape;
         if(tL == null)
         {
            tL = new Shape();
            tL.name = "lines";
            this.addChild(tL);
         }
         tL.graphics.clear();
         if(this.itemCardReference != null && this.background != null && this.Value_tf != null)
         {
            tL.graphics.lineStyle(2,this.itemCardReference.iStyleBgColor,1);
            tL.graphics.moveTo(startX - 3,Math.round(this.background.y));
            tL.graphics.lineTo(startX - 3,Math.round(this.background.y + this.background.height));
         }
         tL.alpha = 1;
      }
   }
}

