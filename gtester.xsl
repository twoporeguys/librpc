<?xml version="1.0"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
    
    <!-- I can't believe I have to do this -->
    <!-- Based on this code:
            http://geekswithblogs.net/Erik/archive/2008/04/01/120915.aspx
    -->
    <xsl:template name="strreplace">
        <xsl:param name="string"/>
        <xsl:param name="token"/>
        <xsl:param name="newtoken"/>
        <xsl:choose>
            <xsl:when test="contains($string, $token)">
                <xsl:value-of select="substring-before($string, $token)"/>
                <xsl:value-of select="$newtoken"/>
                <xsl:call-template name="strreplace">
                    <xsl:with-param name="string" select="substring-after($string, $token)"/>
                    <xsl:with-param name="token" select="$token"/>
                    <xsl:with-param name="newtoken" select="$newtoken"/>
                </xsl:call-template>
            </xsl:when>
            <xsl:otherwise>
                <xsl:value-of select="$string"/>
            </xsl:otherwise>
        </xsl:choose>
    </xsl:template>

    <xsl:template match="/">
        <xsl:for-each select="gtester">
            <testsuite>
                <!-- currently we do not support different test binaries and only take the path
                     of the first as the testsuite name -->
                <xsl:attribute name="name">
                    <xsl:value-of select="testbinary[1]/@path"/>
                </xsl:attribute>
                <xsl:attribute name="tests">
                    <xsl:value-of select="count(testbinary/testcase[not(@skipped='1')])"/>
                </xsl:attribute>
                <xsl:attribute name="time">
                    <xsl:value-of select="sum(testbinary/duration)"/>
                </xsl:attribute>
                <xsl:attribute name="failures">
                    <xsl:value-of select="count(testbinary/testcase/status[@result='failed'])"/>
                </xsl:attribute>
                <xsl:for-each select="testbinary/testcase[not(@skipped='1')]">
                    <testcase>
                        <xsl:variable name="classname">
                            <xsl:call-template name="strreplace">
                                <xsl:with-param name="string" select="substring-after(@path, '/')"/>
                                <xsl:with-param name="token" select="'/'"/>
                                <xsl:with-param name="newtoken" select="'.'"/>
                            </xsl:call-template>
                        </xsl:variable>
                        <xsl:attribute name="classname">
                            <xsl:value-of select="$classname"/>
                        </xsl:attribute>
                        <xsl:attribute name="name">test</xsl:attribute>
                        <xsl:attribute name="time">
                            <xsl:value-of select="duration"/>
                        </xsl:attribute>
                        <!-- Has to be inverted because of https://gitlab.gnome.org/GNOME/glib/issues/1305 -->
                        <xsl:if test="status[@result = 'success']">
                            <failure>
                                <xsl:value-of select="error"/>
                            </failure>
                        </xsl:if>
                    </testcase>
                </xsl:for-each>
            </testsuite>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
