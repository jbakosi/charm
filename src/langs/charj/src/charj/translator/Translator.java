/***
***/

package charj.translator;

import java.io.*;
import org.antlr.runtime.*;
import org.antlr.runtime.tree.*;
import org.antlr.stringtemplate.*;
import charj.translator.StreamEmitter;

/**
 * Indicates whether we are working on .ci or .cc output.
 */
enum OutputMode {
    cc(".cc"), 
    ci(".ci");

    private final String extension;

    OutputMode(String ext) {
        this.extension = ext;
    }

    public String extension() {
        return extension;
    }
}

/**
 * Driver class for lexing, parsing, and output.
 * Takes in file names, parses them and generates code
 * for .ci and .cc files in a .charj directory. Invokes
 * charmc on these outputs and moves any resulting .o file 
 * to the appropriate directory.
 */
public class Translator {

    // template file locations
    public static final String ccTemplateFile = 
        "charj/translator/CharjCC.stg";
    public static final String ciTemplateFile = 
        "charj/translator/CharjCI.stg";

    // variables controlled by command-line arguments
    public String charmc;
    public boolean debug;
    public boolean verbose;
    public boolean errorCondition;

    public Translator(
            String _charmc,
            boolean _debug,
            boolean _verbose)
    {
        charmc = _charmc;
        debug = _debug;
        verbose = _verbose;

        errorCondition = false;
    }

    public String translate(String filename) throws Exception 
    {
        ANTLRFileStream input = new ANTLRFileStream(filename);
            
        CharjLexer lexer = new CharjLexer(input);
        String ciOutput = translationPass(lexer, OutputMode.ci);
        writeTempFile(filename, ciOutput, OutputMode.ci);
        
        input.seek(0);
        String ccOutput = translationPass(lexer, OutputMode.cc);
        writeTempFile(filename, ccOutput, OutputMode.cc);
        compileTempFiles(filename, charmc);

        String ciHeader = "-----CI----------------------------\n";
        String ccHeader = "-----CC----------------------------\n";
        String footer =   "-----------------------------------\n";
        return ciHeader + ciOutput + ccHeader + ccOutput + footer;
    }

    private String translationPass(
            CharjLexer lexer, 
            OutputMode m) throws
        RecognitionException, IOException, InterruptedException
    {
        // Use lexer tokens to feed tree parser.
        CommonTokenStream tokens = new CommonTokenStream(lexer);
        CharjParser parser = new CharjParser(tokens);
        CharjParser.charjSource_return r = parser.charjSource();

        // Create node stream for emitters
        CommonTree t = (CommonTree)r.getTree();
        CommonTreeNodeStream nodes = new CommonTreeNodeStream(t);
        nodes.setTokenStream(tokens);

        String output = null;
        if (m == OutputMode.cc) {
            output = generateCC(nodes);
        } else if (OutputMode.ci == m) {
            output = generateCI(nodes);
        }
        return output;
    }

    /**
     * Utility function to write a generated .ci or .cc
     * file to disk. Takes a .cj filename and writes a .cc
     * or .ci file to the .charj directory depending on
     * the OutputMode.
     */
    private void writeTempFile(
            String filename, 
            String output,
            OutputMode m) throws
        IOException
    {
        int lastDot = filename.lastIndexOf(".");
        int lastSlash = filename.lastIndexOf("/");
        String tempFile = filename.substring(0, lastSlash + 1) + ".charj/";
        new File(tempFile).mkdir();
        tempFile += filename.substring(lastSlash + 1, lastDot) + m.extension();
        if (verbose) System.out.println("\n [charjc] create: " + tempFile);
        FileWriter fw = new FileWriter(tempFile);
        fw.write(output);
        fw.close();
        return;
    }

    /**
     * Enters the .charj directory and compiles the .cc and .ci files 
     * generated from the given filename. The given charmc string 
     * include all options to be passed to charmc. Any generated .o 
     * file is moved back to the initial directory.
     */
    private void compileTempFiles(
            String filename,
            String charmc) throws
        IOException, InterruptedException
    {
        int lastDot = filename.lastIndexOf(".");
        int lastSlash = filename.lastIndexOf("/");
        String baseFilename = filename.substring(0, lastSlash + 1) + 
            ".charj/" + filename.substring(lastSlash + 1, lastDot);
        String cmd = charmc + " " + baseFilename + ".ci";
        File currentDir = new File(".");
        int retVal = exec(cmd, currentDir);
        if (retVal != 0) return;
        
        cmd = charmc + " -c " + baseFilename + ".cc";
        retVal = exec(cmd, currentDir);
        if (retVal != 0) return;

        cmd = "mv -f " + baseFilename + ".o" + " .";
        exec(cmd, currentDir);
    }

    /**
     * Utility function to execute a given command line.
     */
    private int exec(String cmd, File outputDir) throws
        IOException, InterruptedException
    {
        if (verbose) System.out.println("\n [charjc] exec: " + cmd + "\n");
        Process p = Runtime.getRuntime().exec(cmd, null, outputDir);
        StreamEmitter stdout = new StreamEmitter(
                p.getInputStream(), System.out);
        StreamEmitter stderr = new StreamEmitter(
                p.getErrorStream(), System.err);
        stdout.start();
        stderr.start();
        p.waitFor();
        stdout.join();
        stderr.join();
        int retVal = p.exitValue();
        return retVal;
    }

    private String generateCC(CommonTreeNodeStream nodes) throws
        RecognitionException, IOException, InterruptedException
    {
        CharjCCEmitter emitter = new CharjCCEmitter(nodes);
        StringTemplateGroup templates = getTemplates(ccTemplateFile);
        emitter.setTemplateLib(templates);
        StringTemplate st = (StringTemplate)emitter.charjSource().getTemplate();
        return st.toString();
    }

    private String generateCI(CommonTreeNodeStream nodes) throws
        RecognitionException, IOException, InterruptedException
    {
        CharjCIEmitter emitter = new CharjCIEmitter(nodes);
        StringTemplateGroup templates = getTemplates(ciTemplateFile);
        emitter.setTemplateLib(templates);
        StringTemplate st = (StringTemplate)emitter.charjSource().getTemplate();
        return st.toString();
    }

    private StringTemplateGroup getTemplates(String templateFile)
    {
        StringTemplateGroup templates = null;
        try {
            ClassLoader loader = Thread.currentThread().getContextClassLoader();
            InputStream istream = loader.getResourceAsStream(templateFile);
            BufferedReader reader = new BufferedReader(new InputStreamReader(istream));
            templates = new StringTemplateGroup(reader);
            reader.close();
        } catch(IOException ex) {
            error("Failed to load template file", ex); 
        }
        return templates;
    }
    
    private void error(
            String sourceName, 
            String msg, 
            CommonTree node) 
    {
        errorCondition = true;
        String linecol = ":";
        if ( node!=null ) {
            CommonToken t = (CommonToken)node.getToken();
            linecol = "line " + t.getLine() + ":" + t.getCharPositionInLine();
        }
        System.err.println(sourceName + ": " + linecol + " " + msg);
        System.err.flush();
    }


    private void error(
            String sourceName, 
            String msg) {
        error(sourceName, msg, (CommonTree)null);
    }


    public void error(String msg) {
        error("charj", msg, (CommonTree)null);
    }


    public void error(
            String msg, 
            Exception e) {
        error(msg);
        e.printStackTrace(System.err);
    }
}
