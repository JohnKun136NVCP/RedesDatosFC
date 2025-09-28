# Repaso de prácticas con Git y GitHub. 📖

Esperando que la mayoría del curso supiese lo básico de Git. Esto abarca desde:
- Inicialización de un repositorio
- Hacer un Stage en un repositorio
- Crear commits
- Trabajar de repositorios locales y remotos.
Si no es así o si todavía hay dificultad, por favor, ver [ver este enlace](https://git-scm.com/docs).

Ahora bien, el proposito de esta práctica es que sepan en colaborar en equipo, no siempre van a tener un repositorio individual para trabajar, sino trabajar de manera colaborativa (En este caso puede ser de desarrollo Web, software, etc.) Para ello hay pasos sencillos pero importantes:

## Regla de Oro. 🥇
*SIMPRE HACER UN **PULL** ANTES DE UN **PUSH***.
Esta es una simple regla sencilla pero demasiado últil y ahorra estrés junto con los problemas. 
Cuando uno esta trabajando y acabó la parte que le correspondía de su tareas, etc. Debe recordar que no solo es un programad@r que esta trabajando en el repositorio, sino son varios, entonces para actualizar los cambios que se hicieron y subir los cambios que uno hizo simpre usar el comando:

```bash
git pull #Actualiza los cambios que se hicieron.
 ```
Ahora sí despues de hacer los cambios ``git add`` y el ``git commit``, se tiene que hacer:

```bash
git push
```

## Pull Request.
Un pull request es una solicitud de cambios, en los cuales uno va a aportar algo al repositorio. En algunos casos vi que cerraban el Pull Request y creaban otro, pero esto en desarrollo no es una buena práctica, al menos que ya se haya solucionado el error o que no haya forma de solucionarlo se cierra. Esto sirve para no crear demasiados Pull Request del mismo tema y autor. Se recomienda dejar el Pull Request abierto hasta que se acepte la solucitud e ir actualizando la información de lo que se vaya haciendo si hay cambios. Al menos que no funcione lo que se propone, se cerrará el Pull Request.

## Práctica
¿Has escuchado sobre Project Euler? Bien sino, es una plataforma para desarrollar la habilidad tanto de programación (óptimización) como también la matemática.
Por tanto, la práctica será una práctica en conjunto. 

1. Ingresa a [Project Euler](https://projecteuler.net/) y registrate.
2. Escoje un problema el que sea de tu agrado.
3. En el repositorio que el repositorio que se clonó no se tiene esta rama y esta con un Folk, en este caso **antes de hacer cualquier cosa** si entras a tu repositorio que hiciste el Folk verás un botón que dice **Sync Fork**, como dice el nombre sincroniza los cambios del repositorio "Original" con el que tienes. Da click en Sync Folk y te dará los cambios que se han hecho durante este tiempo.
4. Ahí te aparecerá la rama que se creó, entonces lo único que se tiene que hacer desde la terminal de su computadora es actualizar las ramas remotas a las locales, para hacer esto se usa el siguiente comando:
```bash
    git remote update origin --prune
```
Después solamente falta cambiarse de rama usando: 
```bash
    git checkout gitstudy
```
5. Lo divertido viene desde este punto. En este README, deberás agregar tu nombre y tu GitHub. **Opcional:** *Puedes seguir a tus demás compañer@s en GitHub por si te interesa su trabajo o quieres colaborar en alguno*.
6. Además de ello, deberá subir la solución en el directorio donde dice **solutions**, con la siguiente nomenclatura: **Iniciales empezando por apellidos**_EULER_**número del ejercio**. Por ejemplo; `ANJ_EULER_1.cpp`. En este caso, se le dará libertad a cada quien de resolverlo en el lenguaje que le sea más cómodo, sin embargo, pueden tener una participación si lo hacen en C. 
7. En el reporte debe incluir la captura del problema resuelto que está aprobado, la misma página les dará el resultado.

**NOTA (1)**: El truco de esto es el que resuleva más rápido los ejercicios. Por otra parte, se le sugiere hacer el **Sync Folk** constantemente por si llegué a subir cambios al original.

**Nota (2)**: Si quieren poner el enlace directo a su repositorio pueden usar la siguiente sintaxis en Markdown.

```markdown
[Google](www.google.com)
```
El primer parámetro es el texto a mostrar, este tiene que estar en cochetes y los paretesis son para la url que va contener, en este caso era de Google.

======

Desde aquí puedes ir empezando.


Hola soy John y mi GitHub es [este](https://github.com/JohnKun136NVCP) por si me quieres seguir o te interesan mis proyectos.

¡Qué tal! Soy Eduardo Castillo y estoy en GitHub como [PlexusDeus](https://github.com/PlexusDeus)

Hola soy Andres, mi Github es [este](https://github.com/AndresCataneo), resolví el problema 10 en C, que trata sobre encontrar la suma de los numeros primos menores a 2 millones.

Hola, soy Alejandro Jacome Delgado y mi GitHub es [kyogre235](https://github.com/kyogre235).

Hola, soy Diego, mi Github es [este](https://github.com/Sloot25), resolvi el problema 5 en C, que trata sobre encontrar el multiplo mas pequeño de todos los numeros desde el 1 al 20.

======
