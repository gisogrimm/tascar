% tascar_xml_get_element - Get a handle to the first element that matches the specified criteria.
%
% Usage:
%   h = tascar_xml_get_element(doc, etype, [cattr1, cvalue1, ...]);
%
% Inputs:
%   doc - A Java Document object or an existing element to search within.
%   etype - The type of the element to search for (e.g., 'tagname').
%   varargin - Optional constraint attributes and values to filter the elements to be returned.
%     cattr1 - The name of the first constraint attribute.
%     cvalue1 - The value of the first constraint attribute.
%     ...
%
% Outputs:
%   h - A cell array containing the first element that matches the specified criteria.
%
function h = tascar_xml_get_element(doc, etype, varargin)
% Check if the input document and element type are valid
    if ~isjava(doc)
        error('Input document must be a Java Document object.');
    end
    if ~ischar(etype)
        error('Element type must be a string.');
    end

    % Get a list of all elements with the specified type under the parent node.
    % This list is used to iterate over the elements and find the first one that matches the constraint attributes.
    elem_list = javaMethod('getElementsByTagName', doc, etype);

    % Get the number of elements in the list.
    N = javaMethod('getLength', elem_list);

    % Initialize an empty cell array to store the handle to the first matching element.
    h = {};

    % Iterate over the elements in the list.
    for k = 1:N
        % Get the current element from the list.
        elem = javaMethod('item', elem_list, k-1);

        % Initialize a flag to indicate whether the element matches the constraint attributes.
        b_matched = true;

        % Iterate over the constraint attributes and values.
        for katt = 1:2:numel(varargin)
            % Get the attribute value of the current element.
            attr_value = javaMethod('getAttribute', elem, varargin{katt});

            % Check if the attribute value matches the constraint value.
            if ~strcmp(attr_value, varargin{katt+1})
                % If the attribute value does not match, set the flag to false.
                b_matched = false;
                break;
            end
        end

        % If the element matches the constraint attributes, add its handle to the cell array.
        if b_matched
            h{end+1} = elem;
        end
    end
end
