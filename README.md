# messagerieInstantanee

Pour lancer le fork serveur : 
- aller dans le dossier echo_server
- dans une console, entrer la commande
```bash
make fork_server 
```
- pour lancer le serveur, faire dans la console
```bash
./fork_server -un numero de port-
```
- pour connecter un client au serveur, dans une autre console, faire
```bash
nc localhost -le numero de port choisi-
```

Ce serveur renvoie a l'utilisateur son propre message

