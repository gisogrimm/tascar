function doc = tascar_xml_doc_new
% tascar_xml_doc_new - create new xml document
%
% doc = tascar_xml_doc_new();
    factory = javaMethod('newInstance', ...
                         'javax.xml.parsers.DocumentBuilderFactory');
    docBuilder = javaMethod('newDocumentBuilder',factory);
    doc = javaMethod('newDocument',docBuilder);
end
