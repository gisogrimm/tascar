% tascar_xml_rm_element - Remove a sub-element from an XML element.
%
% Usage:
%   tascar_xml_rm_element(pNode, etype, [cattr1, cvalue1, ...]);
%
% Inputs:
%   pNode - The parent node of the element to be removed.
%   etype - The type of the element to be removed (e.g., 'tagname').
%   varargin - Optional constraint attributes and values to filter the elements to be removed.
%     cattr1 - The name of the first constraint attribute.
%     cvalue1 - The value of the first constraint attribute.
%     ...
%
% Outputs:
%   None
%
function tascar_xml_rm_element(pNode, etype, varargin)
    if iscell(pNode)
        for k=1:numel(pNode)
            tascar_xml_rm_element(pNode{k}, etype, varargin{:})
        end
        return
    end
    % Check if the input parent node and element type are valid
    if ~isjava(pNode)
        error('Input parent node must be a Java XML Element object.');
    end
    if ~ischar(etype)
        error('Element type must be a string.');
    end

    % Get a list of all elements with the specified type under the parent node.
    % This list is used to iterate over the elements and remove the ones that match the constraint attributes.
    elem_list = javaMethod('getElementsByTagName', pNode, etype);

    % Get the number of elements in the list.
    N = javaMethod('getLength', elem_list);

    % Iterate over the elements in reverse order to avoid index shifting when removing elements.
    for k = N:-1:1
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

        % If the element matches the constraint attributes, remove it from the parent node.
        if b_matched
            javaMethod('removeChild', pNode, elem);
        end
    end
end
