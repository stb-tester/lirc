<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <!-- Default: copy everything. -->
  <xsl:template match="node()|@*">
      <xsl:copy>
        <xsl:apply-templates/>
      </xsl:copy>
  </xsl:template>

  <!-- When copying <a>, include attributes. -->
  <xsl:template match="a">
    <a>
      <xsl:copy-of select="@*|b/@*" />
      <xsl:apply-templates />
    </a>
  </xsl:template>

  <!-- When copying <th>, include attributes. -->
  <xsl:template match="th">
    <th>
      <xsl:copy-of select="@*|b/@*" />
      <xsl:apply-templates />
    </th>
  </xsl:template>

  <!-- Wrap text in the root of <body> in <p> tags. -->
  <xsl:template match="body/text()[string-length(current()) > 10]" >
    <p><xsl:copy/></p>
  </xsl:template>

  <!-- Main layout: top menu, the generated page and a footer. -->
  <xsl:template match="body">
    <body>
      <TABLE CLASS="menu">
        <TR>
          <TD CLASS="menu">
            <IMG class="menuimg"
             SRC="../images/diode.gif" ALT="LIRC icon" ALIGN="LEFT"/>
           </TD>
           <TD>
            <IMG class="menuimg"
             SRC="../images/lirc.gif" ALT="LIRC logo" ALIGN="RIGHT"/>
          </TD>
        </TR>
      </TABLE>
      <xsl:apply-templates/>
      <p class="footer">
        [<A HREF="http://www.lirc.org/">LIRC homepage</A>]
      </p>
    </body>
  </xsl:template>

  <!-- Top level: Use fixed <head>, insert the rest as <body>. -->
  <xsl:template match="/html">
    <html>
      <HEAD>
        <TITLE>LIRC - Linux Infrared Remote Control</TITLE>
        <LINK REL="stylesheet" TYPE="text/css" HREF="lirc.css"/>
        <LINK REL="shortcut icon" HREF="../images/favicon.ico"/>
        <META NAME="description"
              CONTENT="LIRC - Linux Infrared Remote Control"/>
        <META NAME="keywords" CONTENT="linux remote control, multimedia"/>
        <META charset="UTF-8"/>
      </HEAD>
      <xsl:apply-templates/>
    </html>
  </xsl:template>

</xsl:stylesheet>
