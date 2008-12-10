<?xml version="1.0"?>

<!--
 *  A template to generate a generic IDL file from the generic interface
 *  definition expressed in XML. The generated file is intended solely to
 *  generate the documentation using Doxygen.

     Copyright (C) 2006-2007 Sun Microsystems, Inc.

     This file is part of VirtualBox Open Source Edition (OSE), as
     available from http://www.virtualbox.org. This file is free software;
     you can redistribute it and/or modify it under the terms of the GNU
     General Public License (GPL) as published by the Free Software
     Foundation, in version 2 as it comes in the "COPYING" file of the
     VirtualBox OSE distribution. VirtualBox OSE is distributed in the
     hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

     Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
     Clara, CA 95054 USA or visit http://www.sun.com if you need
     additional information or have any questions.
-->

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" indent="yes"/>

<xsl:strip-space elements="*"/>


<!--
//  helper definitions
/////////////////////////////////////////////////////////////////////////////
-->

<!--
 *  uncapitalizes the first letter only if the second one is not capital
 *  otherwise leaves the string unchanged
-->
<xsl:template name="uncapitalize">
  <xsl:param name="str" select="."/>
  <xsl:choose>
    <xsl:when test="not(contains('ABCDEFGHIJKLMNOPQRSTUVWXYZ', substring($str,2,1)))">
      <xsl:value-of select="
        concat(
          translate(substring($str,1,1),'ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz'),
          substring($str,2)
        )
      "/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="string($str)"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<!--
 *  translates the string to uppercase
-->
<xsl:template name="uppercase">
  <xsl:param name="str" select="."/>
  <xsl:value-of select="
    translate($str,'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')
  "/>
</xsl:template>


<!--
//  Doxygen transformation rules
/////////////////////////////////////////////////////////////////////////////
-->

<!--
 *  all text elements that are not explicitly matched are normalized
 *  (all whitespace chars are converted to single spaces)
-->
<!--xsl:template match="desc//text()">
    <xsl:value-of select="concat(' ',normalize-space(.),' ')"/>
</xsl:template-->

<!--
 *  all elements that are not explicitly matched are considered to be html tags
 *  and copied w/o modifications
-->
<xsl:template match="desc//*">
  <xsl:copy>
    <xsl:apply-templates/>
  </xsl:copy>
</xsl:template>

<!--
 *  paragraph
-->
<xsl:template match="desc//p">
  <xsl:text>&#x0A;</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>&#x0A;</xsl:text>
</xsl:template>

<!--
 *  link
-->
<xsl:template match="desc//link">
  <xsl:text>@link </xsl:text>
  <!--
   *  sometimes Doxygen is stupid and cannot resolve global enums properly,
   *  thinking they are members of the current class. Fix it by adding ::
   *  in front of any @to value that doesn't start with #.
  -->
  <xsl:choose>
    <xsl:when test="not(starts-with(@to, '#')) and not(contains(@to, '::'))">
      <xsl:text>::</xsl:text>
    </xsl:when>
  </xsl:choose>
  <!--
   *  Doxygen doesn't understand autolinks like Class::func() if Class
   *  doesn't actually contain a func with no arguments. Fix it.
  -->
  <xsl:choose>
    <xsl:when test="substring(@to, string-length(@to)-1)='()'">
      <xsl:value-of select="substring-before(@to, '()')"/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="@to"/>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:text> </xsl:text>
  <xsl:choose>
    <xsl:when test="normalize-space(text())">
      <xsl:value-of select="normalize-space(text())"/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:choose>
        <xsl:when test="starts-with(@to, '#')">
          <xsl:value-of select="substring-after(@to, '#')"/>
        </xsl:when>
        <xsl:when test="starts-with(@to, '::')">
          <xsl:value-of select="substring-after(@to, '::')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="@to"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:text>@endlink</xsl:text>
  <!--
   *  insert a dummy empty B element to distinctly separate @endlink
   *  from the following text
   -->
  <xsl:element name="b"/>
</xsl:template>

<!--
 *  note
-->
<xsl:template match="desc/note">
  <xsl:if test="not(@internal='yes')">
    <xsl:text>&#x0A;@note </xsl:text>
    <xsl:apply-templates/>
    <xsl:text>&#x0A;</xsl:text>
  </xsl:if>
</xsl:template>

<!--
 *  see
-->
<xsl:template match="desc/see">
  <xsl:text>&#x0A;@see </xsl:text>
  <xsl:apply-templates/>
  <xsl:text>&#x0A;</xsl:text>
</xsl:template>


<!--
 *  common comment prologue (handles group IDs)
-->
<xsl:template match="desc" mode="begin">
  <xsl:param name="id" select="@group | preceding::descGroup[1]/@id"/>
  <xsl:text>/**&#x0A;</xsl:text>
  <xsl:if test="$id">
    <xsl:value-of select="concat(' @ingroup ',$id,'&#x0A;')"/>
  </xsl:if>
</xsl:template>

<!--
 *  common brief comment prologue (handles group IDs)
-->
<xsl:template match="desc" mode="begin_brief">
  <xsl:param name="id" select="@group | preceding::descGroup[1]/@id"/>
  <xsl:text>/**&#x0A;</xsl:text>
  <xsl:if test="$id">
    <xsl:value-of select="concat(' @ingroup ',$id,'&#x0A;')"/>
  </xsl:if>
  <xsl:text> @brief&#x0A;</xsl:text>
</xsl:template>

<!--
 *  common middle part of the comment block
-->
<xsl:template match="desc" mode="middle">
  <xsl:apply-templates select="text() | *[not(self::note or self::see)]"/>
  <xsl:apply-templates select="note"/>
  <xsl:apply-templates select="see"/>
</xsl:template>

<!--
 *  result part of the comment block
-->
<xsl:template match="desc" mode="results">
  <xsl:if test="result">
    <xsl:text>
      @par Expected result codes:
    </xsl:text>
      <table>
    <xsl:for-each select="result">
      <tr>
        <xsl:choose>
          <xsl:when test="ancestor::library/result[@name=current()/@name]">
            <td><xsl:value-of select=
                  "concat('@link ::',@name,' ',@name,' @endlink')"/></td>
          </xsl:when>
          <xsl:otherwise>
            <td><xsl:value-of select="@name"/></td>
          </xsl:otherwise>
        </xsl:choose>
        <td>
          <xsl:apply-templates select="text() | *[not(self::note or self::see or
                                                  self::result)]"/>
        </td>
      </tr>
    </xsl:for-each>
      </table>
  </xsl:if>
</xsl:template>


<!--
 *  comment for interfaces
-->
<xsl:template match="interface/desc">
  <xsl:apply-templates select="." mode="begin"/>
  <xsl:apply-templates select="." mode="middle"/>
@par Interface ID:
<tt>{<xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="../@uuid"/>
  </xsl:call-template>}</tt>
  <xsl:text>&#x0A;*/&#x0A;</xsl:text>
</xsl:template>

<!--
 *  comment for attributes
-->
<xsl:template match="attribute/desc">
  <xsl:apply-templates select="." mode="begin"/>
  <xsl:apply-templates select="text() | *[not(self::note or self::see or self::result)]"/>
  <xsl:apply-templates select="." mode="results"/>
  <xsl:apply-templates select="note"/>
  <xsl:if test="../@mod='ptr'">
    <xsl:text>

@warning This attribute is non-scriptable. In particular, this also means that an
attempt to get or set it from a process other than the process that has created and
owns the object will most likely fail or crash your application.
</xsl:text>
  </xsl:if>
  <xsl:apply-templates select="see"/>
  <xsl:text>&#x0A;*/&#x0A;</xsl:text>
</xsl:template>

<!--
 *  comment for methods
-->
<xsl:template match="method/desc">
  <xsl:apply-templates select="." mode="begin"/>
  <xsl:apply-templates select="text() | *[not(self::note or self::see or self::result)]"/>
  <xsl:for-each select="../param">
    <xsl:apply-templates select="desc"/>
  </xsl:for-each>
  <xsl:apply-templates select="." mode="results"/>
  <xsl:apply-templates select="note"/>
  <xsl:apply-templates select="../param/desc/note"/>
  <xsl:if test="../param/@mod='ptr'">
    <xsl:text>

@warning This method is non-scriptable. In particular, this also means that an
attempt to call it from a process other than the process that has created and
owns the object will most likely fail or crash your application.
</xsl:text>
  </xsl:if>
  <xsl:apply-templates select="see"/>
  <xsl:text>&#x0A;*/&#x0A;</xsl:text>
</xsl:template>

<!--
 *  comment for method parameters
-->
<xsl:template match="method/param/desc">
  <xsl:text>&#x0A;@param </xsl:text>
  <xsl:value-of select="../@name"/>
  <xsl:text> </xsl:text>
  <xsl:apply-templates select="text() | *[not(self::note or self::see)]"/>
  <xsl:text>&#x0A;</xsl:text>
</xsl:template>

<!--
 *  comment for enums
-->
<xsl:template match="enum/desc">
  <xsl:apply-templates select="." mode="begin"/>
  <xsl:apply-templates select="." mode="middle"/>
@par Interface ID:
<tt>{<xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="../@uuid"/>
  </xsl:call-template>}</tt>
  <xsl:text>&#x0A;*/&#x0A;</xsl:text>
</xsl:template>

<!--
 *  comment for enum values
-->
<xsl:template match="enum/const/desc">
  <xsl:apply-templates select="." mode="begin_brief"/>
  <xsl:apply-templates select="." mode="middle"/>
  <xsl:text>&#x0A;*/&#x0A;</xsl:text>
</xsl:template>

<!--
 *  comment for result codes
-->
<xsl:template match="result/desc">
  <xsl:apply-templates select="." mode="begin_brief"/>
  <xsl:apply-templates select="." mode="middle"/>
  <xsl:text>&#x0A;*/&#x0A;</xsl:text>
</xsl:template>

<!--
 *  ignore descGroups by default (processed in /idl)
-->
<xsl:template match="descGroup"/>

<!--
//  templates
/////////////////////////////////////////////////////////////////////////////
-->


<!--
 *  header
-->
<xsl:template match="/idl">
/*
 *  DO NOT EDIT! This is a generated file.
 *
 *  Doxygen IDL definition for VirtualBox Main API (COM interfaces)
 *  generated from XIDL (XML interface definition).
 *
 *  Source    : src/VBox/Main/idl/VirtualBox.xidl
 *  Generator : src/VBox/Main/idl/doxygen.xsl
 *
 *  This IDL is generated using some generic OMG IDL-like syntax SOLELY
 *  for the purpose of generating the documentation using Doxygen and
 *  is not syntactically valid.
 *
 *  DO NOT USE THIS HEADER IN ANY OTHER WAY!
 */

  <!-- general description -->
  <xsl:text>/** @mainpage &#x0A;</xsl:text>
  <xsl:apply-templates select="desc" mode="middle"/>
  <xsl:text>&#x0A;*/&#x0A;</xsl:text>

  <!-- group (module) definitions -->
  <xsl:for-each select="//descGroup">
    <xsl:if test="@id and (@title or desc)">
      <xsl:value-of select="concat('/** @defgroup ',@id,' ',@title)"/>
      <xsl:apply-templates select="desc" mode="middle"/>
      <xsl:text>&#x0A;*/&#x0A;</xsl:text>
    </xsl:if>
  </xsl:for-each>

  <!-- everything else -->
  <xsl:apply-templates select="*[not(self::desc)]"/>

</xsl:template>


<!--
 *  accept all <if>s
-->
<xsl:template match="if">
  <xsl:apply-templates/>
</xsl:template>


<!--
 *  cpp_quote (ignore)
-->
<xsl:template match="cpp">
</xsl:template>


<!--
 *  #ifdef statement (@if attribute)
-->
<xsl:template match="@if" mode="begin">
  <xsl:text>#if </xsl:text>
  <xsl:value-of select="."/>
  <xsl:text>&#x0A;</xsl:text>
</xsl:template>
<xsl:template match="@if" mode="end">
  <xsl:text>#endif&#x0A;</xsl:text>
</xsl:template>


<!--
 *  libraries
-->
<xsl:template match="library">
  <!-- result codes -->
  <xsl:for-each select="result">
    <xsl:apply-templates select="."/>
  </xsl:for-each>
  <!-- all enums go first -->
  <xsl:apply-templates select="enum | if/enum"/>
  <!-- everything else but result codes and enums -->
  <xsl:apply-templates select="*[not(self::result or self::enum) and
                                 not(self::if[result] or self::if[enum])]"/>
</xsl:template>


<!--
 *  result codes
-->
<xsl:template match="result">
  <xsl:apply-templates select="@if" mode="begin"/>
  <xsl:apply-templates select="desc"/>
  <xsl:value-of select="concat('const HRESULT ',@name,' = ',@value,';')"/>
  <xsl:text>&#x0A;</xsl:text>
  <xsl:apply-templates select="@if" mode="end"/>
</xsl:template>


<!--
 *  interfaces
-->
<xsl:template match="interface">
  <xsl:apply-templates select="desc"/>
  <xsl:text>interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> : </xsl:text>
  <xsl:value-of select="@extends"/>
  <xsl:text>&#x0A;{&#x0A;</xsl:text>
  <!-- attributes (properties) -->
  <xsl:apply-templates select="attribute"/>
  <!-- methods -->
  <xsl:apply-templates select="method"/>
  <!-- 'if' enclosed elements, unsorted -->
  <xsl:apply-templates select="if"/>
  <!-- -->
  <xsl:text>}; /* interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> */&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  attributes
-->
<xsl:template match="interface//attribute | collection//attribute">
  <xsl:if test="@array">
    <xsl:message terminate="yes">
      <xsl:value-of select="concat(../../@name,'::',../@name,'::',@name,': ')"/>
      <xsl:text>'array' attributes are not supported, use 'safearray="yes"' instead.</xsl:text>
    </xsl:message>
  </xsl:if>
  <xsl:apply-templates select="@if" mode="begin"/>
  <xsl:apply-templates select="desc"/>
  <xsl:text>    </xsl:text>
  <xsl:if test="@readonly='yes'">
    <xsl:text>readonly </xsl:text>
  </xsl:if>
  <xsl:text>attribute </xsl:text>
  <xsl:apply-templates select="@type"/>
  <xsl:text> </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>;&#x0A;</xsl:text>
  <xsl:apply-templates select="@if" mode="end"/>
  <xsl:text>&#x0A;</xsl:text>
</xsl:template>

<!--
 *  methods
-->
<xsl:template match="interface//method | collection//method">
  <xsl:apply-templates select="@if" mode="begin"/>
  <xsl:apply-templates select="desc"/>
  <xsl:text>    void </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:if test="param">
    <xsl:text> (&#x0A;</xsl:text>
    <xsl:for-each select="param [position() != last()]">
      <xsl:text>        </xsl:text>
      <xsl:apply-templates select="."/>
      <xsl:text>,&#x0A;</xsl:text>
    </xsl:for-each>
    <xsl:text>        </xsl:text>
    <xsl:apply-templates select="param [last()]"/>
    <xsl:text>&#x0A;    );&#x0A;</xsl:text>
  </xsl:if>
  <xsl:if test="not(param)">
    <xsl:text>();&#x0A;</xsl:text>
  </xsl:if>
  <xsl:apply-templates select="@if" mode="end"/>
  <xsl:text>&#x0A;</xsl:text>
</xsl:template>


<!--
 *  co-classes
-->
<xsl:template match="module/class">
  <!-- class and contract id: later -->
  <!-- CLSID_xxx declarations for XPCOM, for compatibility with Win32: later -->
</xsl:template>


<!--
 *  enumerators
-->
<xsl:template match="enumerator">
  <xsl:text>interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> : $unknown&#x0A;{&#x0A;</xsl:text>
  <!-- HasMore -->
  <xsl:text>    void hasMore ([retval] out boolean more);&#x0A;&#x0A;</xsl:text>
  <!-- GetNext -->
  <xsl:text>    void getNext ([retval] out </xsl:text>
  <xsl:apply-templates select="@type"/>
  <xsl:text> next);&#x0A;&#x0A;</xsl:text>
  <!-- -->
  <xsl:text>}; /* interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> */&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  collections
-->
<xsl:template match="collection">
  <xsl:if test="not(@readonly='yes')">
    <xsl:message terminate="yes">
      <xsl:value-of select="concat(@name,': ')"/>
      <xsl:text>non-readonly collections are not currently supported</xsl:text>
    </xsl:message>
  </xsl:if>
  <xsl:text>interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> : $unknown&#x0A;{&#x0A;</xsl:text>
  <!-- Count -->
  <xsl:text>    readonly attribute unsigned long count;&#x0A;&#x0A;</xsl:text>
  <!-- GetItemAt -->
  <xsl:text>    void getItemAt (in unsigned long index, [retval] out </xsl:text>
  <xsl:apply-templates select="@type"/>
  <xsl:text> item);&#x0A;&#x0A;</xsl:text>
  <!-- Enumerate -->
  <xsl:text>    void enumerate ([retval] out </xsl:text>
  <xsl:apply-templates select="@enumerator"/>
  <xsl:text> enumerator);&#x0A;&#x0A;</xsl:text>
  <!-- other extra attributes (properties) -->
  <xsl:apply-templates select="attribute"/>
  <!-- other extra methods -->
  <xsl:apply-templates select="method"/>
  <!-- 'if' enclosed elements, unsorted -->
  <xsl:apply-templates select="if"/>
  <!-- -->
  <xsl:text>}; /* interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> */&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  enums
-->
<xsl:template match="enum">
  <xsl:apply-templates select="desc"/>
  <xsl:text>enum </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>&#x0A;{&#x0A;</xsl:text>
  <xsl:for-each select="const">
    <xsl:apply-templates select="desc"/>
    <xsl:text>    </xsl:text>
    <xsl:value-of select="../@name"/>
    <xsl:text>_</xsl:text>
    <xsl:value-of select="@name"/> = <xsl:value-of select="@value"/>
    <xsl:text>,&#x0A;</xsl:text>
  </xsl:for-each>
  <xsl:text>};&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  method parameters
-->
<xsl:template match="method/param">
  <xsl:if test="@array">
    <xsl:if test="@dir='return'">
      <xsl:message terminate="yes">
        <xsl:value-of select="concat(../../@name,'::',../@name,'::',@name,': ')"/>
        <xsl:text>return 'array' parameters are not supported, use 'safearray="yes"' instead.</xsl:text>
      </xsl:message>
    </xsl:if>
    <xsl:text>[array, </xsl:text>
    <xsl:choose>
      <xsl:when test="../param[@name=current()/@array]">
        <xsl:if test="../param[@name=current()/@array]/@dir != @dir">
          <xsl:message terminate="yes">
            <xsl:value-of select="concat(../../@name,'::',../@name,': ')"/>
            <xsl:value-of select="concat(@name,' and ',../param[@name=current()/@array]/@name)"/>
            <xsl:text> must have the same direction</xsl:text>
          </xsl:message>
        </xsl:if>
        <xsl:text>size_is(</xsl:text>
        <xsl:if test="@dir='out'">
          <xsl:text>, </xsl:text>
        </xsl:if>
        <xsl:if test="../param[@name=current()/@array]/@dir='out'">
          <xsl:text>*</xsl:text>
        </xsl:if>
        <xsl:value-of select="@array"/>
        <xsl:text>)</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:message terminate="yes">
          <xsl:value-of select="concat(../../@name,'::',../@name,'::',@name,': ')"/>
          <xsl:text>array attribute refers to non-existent param: </xsl:text>
          <xsl:value-of select="@array"/>
        </xsl:message>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>] </xsl:text>
  </xsl:if>
  <xsl:choose>
    <xsl:when test="@dir='in'">in </xsl:when>
    <xsl:when test="@dir='out'">out </xsl:when>
    <xsl:when test="@dir='return'">[retval] out </xsl:when>
    <xsl:otherwise>in</xsl:otherwise>
  </xsl:choose>
  <xsl:apply-templates select="@type"/>
  <xsl:text> </xsl:text>
  <xsl:value-of select="@name"/>
</xsl:template>


<!--
 *  attribute/parameter type conversion
-->
<xsl:template match="
  attribute/@type | param/@type |
  enumerator/@type | collection/@type | collection/@enumerator
">
  <xsl:variable name="self_target" select="current()/ancestor::if/@target"/>

  <xsl:if test="../@array and ../@safearray='yes'">
    <xsl:message terminate="yes">
      <xsl:value-of select="concat(../../../@name,'::',../../@name,'::',../@name,': ')"/>
      <xsl:text>either 'array' or 'safearray="yes"' attribute is allowed, but not both!</xsl:text>
    </xsl:message>
  </xsl:if>

  <xsl:choose>
    <!-- modifiers (ignored for 'enumeration' attributes)-->
    <xsl:when test="name(current())='type' and ../@mod">
      <xsl:choose>
        <xsl:when test="../@mod='ptr'">
          <xsl:choose>
            <!-- standard types -->
            <!--xsl:when test=".='result'">??</xsl:when-->
            <xsl:when test=".='boolean'">booleanPtr</xsl:when>
            <xsl:when test=".='octet'">octetPtr</xsl:when>
            <xsl:when test=".='short'">shortPtr</xsl:when>
            <xsl:when test=".='unsigned short'">ushortPtr</xsl:when>
            <xsl:when test=".='long'">longPtr</xsl:when>
            <xsl:when test=".='long long'">llongPtr</xsl:when>
            <xsl:when test=".='unsigned long'">ulongPtr</xsl:when>
            <xsl:when test=".='unsigned long long'">ullongPtr</xsl:when>
            <xsl:when test=".='char'">charPtr</xsl:when>
            <!--xsl:when test=".='string'">??</xsl:when-->
            <xsl:when test=".='wchar'">wcharPtr</xsl:when>
            <!--xsl:when test=".='wstring'">??</xsl:when-->
            <xsl:otherwise>
              <xsl:message terminate="yes">
                <xsl:value-of select="concat(../../../@name,'::',../../@name,'::',../@name,': ')"/>
                <xsl:text>attribute 'mod=</xsl:text>
                <xsl:value-of select="concat('&quot;',../@mod,'&quot;')"/>
                <xsl:text>' cannot be used with type </xsl:text>
                <xsl:value-of select="concat('&quot;',current(),'&quot;!')"/>
              </xsl:message>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
            <xsl:value-of select="concat(../../../@name,'::',../../@name,'::',../@name,': ')"/>
            <xsl:value-of select="concat('value &quot;',../@mod,'&quot; ')"/>
            <xsl:text>of attribute 'mod' is invalid!</xsl:text>
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <!-- no modifiers -->
    <xsl:otherwise>
      <xsl:choose>
        <!-- standard types -->
        <xsl:when test=".='result'">result</xsl:when>
        <xsl:when test=".='boolean'">boolean</xsl:when>
        <xsl:when test=".='octet'">octet</xsl:when>
        <xsl:when test=".='short'">short</xsl:when>
        <xsl:when test=".='unsigned short'">unsigned short</xsl:when>
        <xsl:when test=".='long'">long</xsl:when>
        <xsl:when test=".='long long'">long long</xsl:when>
        <xsl:when test=".='unsigned long'">unsigned long</xsl:when>
        <xsl:when test=".='unsigned long long'">unsigned long long</xsl:when>
        <xsl:when test=".='char'">char</xsl:when>
        <xsl:when test=".='wchar'">wchar</xsl:when>
        <xsl:when test=".='string'">string</xsl:when>
        <xsl:when test=".='wstring'">wstring</xsl:when>
        <!-- UUID type -->
        <xsl:when test=".='uuid'">uuid</xsl:when>
        <!-- system interface types -->
        <xsl:when test=".='$unknown'">$unknown</xsl:when>
        <xsl:otherwise>
          <xsl:choose>
            <!-- enum types -->
            <xsl:when test="
              (ancestor::library/enum[@name=current()]) or
              (ancestor::library/if[@target=$self_target]/enum[@name=current()])
            ">
              <xsl:value-of select="."/>
            </xsl:when>
            <!-- custom interface types -->
            <xsl:when test="
              (name(current())='enumerator' and
               ((ancestor::library/enumerator[@name=current()]) or
                (ancestor::library/if[@target=$self_target]/enumerator[@name=current()]))
              ) or
              ((ancestor::library/interface[@name=current()]) or
               (ancestor::library/if[@target=$self_target]/interface[@name=current()])
              ) or
              ((ancestor::library/collection[@name=current()]) or
               (ancestor::library/if[@target=$self_target]/collection[@name=current()])
              )
            ">
              <xsl:value-of select="."/>
            </xsl:when>
            <!-- other types -->
            <xsl:otherwise>
              <xsl:message terminate="yes">
                <xsl:text>Unknown parameter type: </xsl:text>
                <xsl:value-of select="."/>
              </xsl:message>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:if test="../@safearray='yes'">
    <xsl:text>[]</xsl:text>
  </xsl:if>
</xsl:template>

</xsl:stylesheet>

