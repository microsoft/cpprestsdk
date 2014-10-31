$files=get-childitem Binaries *.pub.pdb -rec | where-object {!($_psiscontainer)} | foreach-object -process { $_.FullName }
foreach($file in $files)
{
    $newFileName = $file -replace '\.pub\.pdb', '.pdb'
    "Renaming $file to $newFileName..."
    remove-item $newFileName
    rename-item $file -newname $newFileName
}

"Finished renaming all stripped pdbs"