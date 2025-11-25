% tascar_xml_open - Open an XML file and return a Java Document object.
%
% Usage:
%   doc = tascar_xml_open(ifname);
%
% Inputs:
%   ifname - Input file name of the XML file to be opened.
%
% Outputs:
%   doc - A Java Document object representing the parsed XML file.
%
function doc = tascar_xml_open(ifname)
% Check if the input is a valid string
    if ~ischar(ifname)
        error('Input file name must be a string.');
    end

    % Create a new instance of the DocumentBuilderFactory class.
    % This class is used to create a new DocumentBuilder instance.
    factory = javaMethod('newInstance', 'javax.xml.parsers.DocumentBuilderFactory');

    % Create a new DocumentBuilder instance using the factory.
    % This builder is used to parse the XML file.
    docBuilder = javaMethod('newDocumentBuilder', factory);

    % Get the directory, name, and extension of the input file.
    % This is used to construct the full path of the file if it's not absolute.
    [sdir, name, ext] = fileparts(ifname);

    % If the input file is not an absolute path, construct the full path.
    % This ensures that the file is opened correctly regardless of the current working directory.
    if isempty(sdir) || (sdir(1) ~= '/')
        ifname = [pwd(), filesep(), ifname];
    end

    try
        % Parse the XML file using the DocumentBuilder instance.
        % The parsed XML file is returned as a Java Document object.
        doc = javaMethod('parse', docBuilder, ifname);
    catch e
        % Close the DocumentBuilder instance to prevent resource leaks
        clear('docBuilder','factory');
        % Handle any exceptions that occur during parsing
        error(e);
    end

    % Close the DocumentBuilder instance to prevent resource leaks
    clear('docBuilder','factory');
end
