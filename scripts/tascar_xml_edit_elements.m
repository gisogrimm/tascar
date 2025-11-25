% tascar_xml_edit_elements - edit XML elements
%
% Usage:
% doc = tascar_xml_edit_elements( doc, etype, attr, value, [cattr1, cvalue1, ...] )
%
% doc - document (java object)
% etype - element name
% attr - attribute name to edit
% value - new value
% cattr1 - constraint attribute name
% cvalue1 - constraint attribute value
%
% Example:
% Open file 'input.xml', change attribute 'id' to value '3' for all <group.../> elements with
% attribute 'name' = 'alpha' and attribute 'id' = '5':
%
% doc = tascar_xml_open('input.xml');
% doc = tascar_xml_edit_elements( doc, 'group', 'id', '3', 'name', 'alpha', 'id', '5'
% tascar_xml_save(doc, 'output.xml');
%
function doc = tascar_xml_edit_elements( doc, etype, attr, value, varargin )
    if ~isjava(doc)
        error('Input document must be a Java Document object.');
    end
    if ~ischar(etype)
        error('Element type must be a string.');
    end
    if ~ischar(value)
        value = sprintf('%g',value);
    end
    root = javaMethod('getFirstChild',doc);
    elem_list = javaMethod('getElementsByTagName',doc,etype);
    N = javaMethod('getLength',elem_list);
    for k=1:N
        elem = javaMethod('item',elem_list,k-1);
        b_matched = true;
        for k=1:2:numel(varargin)
            if ~strcmp(javaMethod('getAttribute',elem,varargin{k}),varargin{k+1})
                b_matched = false;
            end
        end
        if b_matched
            javaMethod('setAttribute',elem,attr,value);
        end
    end
end
