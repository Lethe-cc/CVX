function v = cvx_pushvar( args )

global cvx___
try
    pstr = cvx___.problems(end);
catch
    error( 'CVX:NoModel', 'No CVX model is present.' );
end

name = args(1).name;
if ~isvarname( name ),
    error( 'CVX:Variable', 'Invalid variable name: %s', name );
elseif isfield( pstr.variables, name ),
    error( 'CVX:Variable', 'Variable name conflict: %s', name );
elseif isfield( pstr.duals,name ),
    error( 'CVX:Variable', 'Primal/dual variable name conflict: %s', name );
end

%
% Parse the structure
%

xsiz = args(1).args;
if isempty( xsiz ),
    xsiz = [ 1, 1 ];
end
isepi  = false;
ishypo = false;
isnneg = false;
issemi = false;
asnneg = pstr.gp;
itype  = '';
iseh   = 0;
if numel( args ) > 1,
    try
        [ str, itype, stypes ] = cvx_create_structure( args );
    catch exc
        error( exc.identifier, exc.message );
    end
    for k = 1 : length( stypes )
        strs = stypes{k};
        switch strs
            case 'epigraph_',    isepi  = true; iseh = +1;
            case 'hypograph_',   ishypo = true; iseh = -1;
            case 'nonnegative',  isnneg = true; asnneg = true;
            case 'semidefinite', issemi = true;
            case 'nonnegative_', asnneg = true;
        end
    end
    if isepi || ishypo,
        if ~isempty( pstr.objective ),
            error( 'CVX:Variable', 'An objective has already been supplied for this problem.' );
        elseif isnan( pstr.direction ),
            error( 'CVX:Variable', 'Epigraph/hypograph variables cannot be added to sets.' );
        elseif ~isreal( str ) || ~isempty( itype ),
            error( 'CVX:Variable', 'Epigraph/hypograph variables must be real and continuous.' );
        elseif ~isempty( str ),
            error( 'CVX:Variable', 'Epigraph/hypograph variables must not have matrix structure.' );
        end
    end
    if pstr.gp
        if issemi
            error( 'CVX:Variable', 'SEMIDEFINITE variables cannot be declared in geometric programs.' );
        elseif ~isreal( str )
            error( 'CVX:Variable', 'Complex variables cannot be declared in geometric programs.' );
        elseif any( nonzeros( str ) ~= 1 )
            error( 'CVX:Variable', 'This matrix structure is incompatibile with geometric programs.' );
        end
    end
else
    str = [];
end

%
% Create the variables
%

if isempty( str ),
    dof = prod( xsiz );
else
    dof = size( str, 1 );
end
tx = cvx_newvar( dof );
if pstr.gp
    tx = cvx_pushexp( tx );
    isnneg = false;
    asnneg = false;
end
v = sparse( tx, 1 : dof, 1 );
if ~isempty( str ), v = v * str; end
v = cvx( xsiz, v );
s1 = xsiz(1);
tn = [];
if isnneg || issemi && s1 == 1,    
    cvx_pushcone( true, 'nonnegative', tx );
    asnneg = false;
    tn = tx;
end
if issemi && s1 > 1,
    if isreal(str), 
        ctype = 'semidefinite';
        mdof = s1*(s1+1)/2;
    else
        ctype = 'hermitian-semidefinite';
        mdof = s1 * s1;
    end
    mmat = prod(xsiz(3:end));
    if ~isnneg && numel( tx ) == mdof * mmat,
        tx = reshape( tx, mdof, mmat );
        cvx_pushcone( true, ctype, tx );
        if ~isnneg,
            if isreal(str)
                tn = tx(cumsum([1,s1:-1:2]),:);
            else
                tn = tx(cumsum([1,2*s1-1:-2:3]),:);
            end
        end
    else
        cvx_pushcnstr( v - semidefinite(xsiz,~isreal(str)), true );
        if ~isnneg,
            tn = reshape( v, s1 * s1, mmat );
            tn = tn( 1 : s1 + 1 : end, : );
            tn = find( any( cvx_basis( tn ), 2 ) );
        end
    end
end
if asnneg
    tn = tx;
end
if ~isempty( itype )
    cvx_pushcone( true, [ 'i_', itype ], tx' );
end
if ~isempty( tn ),
    cvx_setnneg( tn );
end

%
% Add variable to the problem structure
%

if iseh,
    pstr.objective = v;
    pstr.direction = iseh * ( 1 + pstr.gp );
end
pstr.variables.(name) = v;
cvx___.problems( end ) = pstr;


