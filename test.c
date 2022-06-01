#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>

int main(int argc, char const *argv[])
{
  FILE *fic;
  fic = fopen("fichierSalon.txt", "r");
  char *ligne = malloc(100);
  if (fic == NULL)
	{
		fprintf(stderr, "Le fichier 'salon.txt' n'a pas pu être ouvert\n");
		exit(-1);
	}
  printf("ahah\n");
  fgets(ligne, 100, fic);
  printf("ahah\n");
  printf("%s\n", ligne);
  printf("ahah\n");
  free(ligne);
  printf("ahah\n");
  fclose(fic);
  printf("ahah\n");
  return 1;
}