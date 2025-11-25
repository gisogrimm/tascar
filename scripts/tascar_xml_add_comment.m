% tascar_xml_add_comment(doc,elem,sComment)
%
% Adds a comment to the XML document at the specified element.
%
%  doc The XML document object.
%  elem The element at which to insert the comment.
%  sComment The text of the comment to be added.
%
function tascar_xml_add_comment(doc, elem, sComment)
    if ~isjava(doc)
        error('Input document must be a Java Document object.');
    end
    % Create a new comment element with the specified text.
    comment = javaMethod('createComment', doc, sComment);

    % Insert the comment before the specified element.
    javaMethod('insertBefore', javaMethod('getParentNode', elem), comment, elem);
end
