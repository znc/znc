/*
 * ZNC Javascript - http://znc.in
 */
$(document).ready(function()
{
    /**
     * jQ.Poshytip
     * This binds the tooltip to input formelements
     * and uses "title" attr as source for tooltip text.
     */
    $('input[title]').poshytip({
        className: 'tip-yellowsimple',
        showOn: 'focus',
        alignTo: 'target',
        alignX: 'center',
        offsetX: 0,
        offsetY: 5
    });
})