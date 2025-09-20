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
3. En el repositorio que tienes clonado 