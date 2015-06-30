<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output
    doctype-public="-//W3C//DTD HTML 4.01 Transitional//EN"
    doctype-system="http://www.w3.org/TR/html4/loose.dtd"
  />

  <!-- When copying <p>, include attributes. -->
  <xsl:template match="p">
    <p>
      <xsl:copy-of select="@*|b/@*" />
      <xsl:apply-templates />
    </p>
  </xsl:template>

  <!-- Wrap <dt> in <div> so we can apply css margins. -->
  <xsl:template match="dt" >
    <div><dt>
    <xsl:copy-of select="@*|b/@*" />
    <xsl:apply-templates />
    </dt></div>
  </xsl:template>

  <!-- Kill the original <head> -->
  <xsl:template match="head"/>

  <!-- Use the standard page layout -->
  <xsl:include href="page.xsl"/>
</xsl:stylesheet>
