% tascar_xml_save - Save an XML document to a file.
%
% Usage:
%   tascar_xml_save(doc, ofname);
%
% Inputs:
%   doc - A Java Document object representing the XML document to be saved.
%   ofname - The output file name where the XML document will be saved.
%
% Outputs:
%   None
%
function tascar_xml_save(doc, ofname)
% Check if the input document and output file name are valid
    if ~isjava(doc)
        error('Input document must be a Java Document object.');
    end
    if ~ischar(ofname)
        error('Output file name must be a string.');
    end

    % Create a new DOMSource object from the Java Document object.
    % This object is used to transform the XML document into a string.
    dsource = javaObject('javax.xml.transform.dom.DOMSource', doc);

    % Create a new TransformerFactory instance.
    % This factory is used to create a new Transformer instance.
    transformerFactory = javaMethod('newInstance', 'javax.xml.transform.TransformerFactory');

    % Create a new Transformer instance using the factory.
    % This transformer is used to transform the XML document into a string.
    transformer = javaMethod('newTransformer', transformerFactory);

    % Create a new StringWriter object.
    % This object is used to write the transformed XML document to a string.
    writer = javaObject('java.io.StringWriter');

    % Create a new StreamResult object from the StringWriter object.
    % This object is used to write the transformed XML document to the StringWriter.
    result = javaObject('javax.xml.transform.stream.StreamResult', writer);

    % Transform the XML document using the Transformer instance.
    % The transformed XML document is written to the StringWriter.
    javaMethod('transform', transformer, dsource, result);

    % Close the StringWriter object to prevent resource leaks.
    writer.close();

    % Get the transformed XML document as a string.
    xml = char(writer.toString());

    % Split the XML string into a cell array where each row is a cell.
    % This is done to make it easier to write the XML string to a file.
    CellArray = strsplit(xml, '\n');

    % Open the output file in write mode.
    fid = fopen(ofname, 'w');

    % Write each row of the cell array to the file.
    for r = 1:numel(CellArray)
        fprintf(fid, '%s\n', CellArray{r});
    end

    % Close the file to prevent resource leaks.
    fclose(fid);
end
